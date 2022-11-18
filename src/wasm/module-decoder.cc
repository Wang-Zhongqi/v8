// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"

#include "src/logging/metrics.h"
#include "src/wasm/constant-expression.h"
#include "src/wasm/decoder.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

const char* SectionName(SectionCode code) {
  switch (code) {
    case kUnknownSectionCode:
      return "Unknown";
    case kTypeSectionCode:
      return "Type";
    case kImportSectionCode:
      return "Import";
    case kFunctionSectionCode:
      return "Function";
    case kTableSectionCode:
      return "Table";
    case kMemorySectionCode:
      return "Memory";
    case kGlobalSectionCode:
      return "Global";
    case kExportSectionCode:
      return "Export";
    case kStartSectionCode:
      return "Start";
    case kCodeSectionCode:
      return "Code";
    case kElementSectionCode:
      return "Element";
    case kDataSectionCode:
      return "Data";
    case kTagSectionCode:
      return "Tag";
    case kStringRefSectionCode:
      return "StringRef";
    case kDataCountSectionCode:
      return "DataCount";
    case kNameSectionCode:
      return kNameString;
    case kSourceMappingURLSectionCode:
      return kSourceMappingURLString;
    case kDebugInfoSectionCode:
      return kDebugInfoString;
    case kExternalDebugInfoSectionCode:
      return kExternalDebugInfoString;
    case kInstTraceSectionCode:
      return kInstTraceString;
    case kCompilationHintsSectionCode:
      return kCompilationHintsString;
    case kBranchHintsSectionCode:
      return kBranchHintsString;
    default:
      return "<unknown>";
  }
}

// Ideally we'd just say:
//     using ModuleDecoderImpl = ModuleDecoderTemplate<NoTracer>
// but that doesn't work with the forward declaration in the header file.
class ModuleDecoderImpl : public ModuleDecoderTemplate<NoTracer> {
 public:
  ModuleDecoderImpl(WasmFeatures enabled_features,
                    base::Vector<const uint8_t> wire_bytes, ModuleOrigin origin)
      : ModuleDecoderTemplate<NoTracer>(enabled_features, wire_bytes, origin,
                                        no_tracer_) {}

 private:
  NoTracer no_tracer_;
};

ModuleResult DecodeWasmModule(
    WasmFeatures enabled_features, base::Vector<const uint8_t> wire_bytes,
    bool validate_functions, ModuleOrigin origin, Counters* counters,
    std::shared_ptr<metrics::Recorder> metrics_recorder,
    v8::metrics::Recorder::ContextId context_id, DecodingMethod decoding_method,
    AccountingAllocator* allocator) {
  size_t max_size = max_module_size();
  if (wire_bytes.size() > max_size) {
    return ModuleResult{WasmError{0, "size > maximum module size (%zu): %zu",
                                  max_size, wire_bytes.size()}};
  }
  // TODO(bradnelson): Improve histogram handling of size_t.
  auto size_counter =
      SELECT_WASM_COUNTER(counters, origin, wasm, module_size_bytes);
  size_counter->AddSample(static_cast<int>(wire_bytes.size()));
  // Signatures are stored in zone memory, which have the same lifetime
  // as the {module}.
  ModuleDecoderImpl decoder(enabled_features, wire_bytes, origin);
  v8::metrics::WasmModuleDecoded metrics_event;
  base::ElapsedTimer timer;
  timer.Start();
  base::ThreadTicks thread_ticks = base::ThreadTicks::IsSupported()
                                       ? base::ThreadTicks::Now()
                                       : base::ThreadTicks();
  ModuleResult result =
      decoder.DecodeModule(counters, allocator, validate_functions);

  // Record event metrics.
  metrics_event.wall_clock_duration_in_us = timer.Elapsed().InMicroseconds();
  timer.Stop();
  if (!thread_ticks.IsNull()) {
    metrics_event.cpu_duration_in_us =
        (base::ThreadTicks::Now() - thread_ticks).InMicroseconds();
  }
  metrics_event.success = decoder.ok() && result.ok();
  metrics_event.async = decoding_method == DecodingMethod::kAsync ||
                        decoding_method == DecodingMethod::kAsyncStream;
  metrics_event.streamed = decoding_method == DecodingMethod::kSyncStream ||
                           decoding_method == DecodingMethod::kAsyncStream;
  if (result.ok()) {
    metrics_event.function_count = result.value()->num_declared_functions;
  } else if (auto&& module = decoder.shared_module()) {
    metrics_event.function_count = module->num_declared_functions;
  }
  metrics_event.module_size_in_bytes = wire_bytes.size();
  metrics_recorder->DelayMainThreadEvent(metrics_event, context_id);

  return result;
}

ModuleResult DecodeWasmModuleForDisassembler(
    base::Vector<const uint8_t> wire_bytes, AccountingAllocator* allocator) {
  constexpr bool validate_functions = false;
  ModuleDecoderImpl decoder(WasmFeatures::All(), wire_bytes, kWasmOrigin);
  return decoder.DecodeModule(nullptr, allocator, validate_functions);
}

ModuleDecoder::ModuleDecoder(WasmFeatures enabled_features)
    : enabled_features_(enabled_features) {}

ModuleDecoder::~ModuleDecoder() = default;

const std::shared_ptr<WasmModule>& ModuleDecoder::shared_module() const {
  return impl_->shared_module();
}

void ModuleDecoder::StartDecoding(
    Counters* counters, std::shared_ptr<metrics::Recorder> metrics_recorder,
    v8::metrics::Recorder::ContextId context_id, AccountingAllocator* allocator,
    ModuleOrigin origin) {
  DCHECK_NULL(impl_);
  static constexpr base::Vector<const uint8_t> kNoWireBytes{nullptr, 0};
  impl_.reset(new ModuleDecoderImpl(enabled_features_, kNoWireBytes, origin));
  impl_->StartDecoding(counters, allocator);
}

void ModuleDecoder::DecodeModuleHeader(base::Vector<const uint8_t> bytes,
                                       uint32_t offset) {
  impl_->DecodeModuleHeader(bytes, offset);
}

void ModuleDecoder::DecodeSection(SectionCode section_code,
                                  base::Vector<const uint8_t> bytes,
                                  uint32_t offset) {
  impl_->DecodeSection(section_code, bytes, offset);
}

void ModuleDecoder::DecodeFunctionBody(uint32_t index, uint32_t length,
                                       uint32_t offset) {
  impl_->DecodeFunctionBody(index, length, offset);
}

void ModuleDecoder::StartCodeSection(WireBytesRef section_bytes) {
  impl_->StartCodeSection(section_bytes);
}

bool ModuleDecoder::CheckFunctionsCount(uint32_t functions_count,
                                        uint32_t error_offset) {
  return impl_->CheckFunctionsCount(functions_count, error_offset);
}

ModuleResult ModuleDecoder::FinishDecoding() { return impl_->FinishDecoding(); }

size_t ModuleDecoder::IdentifyUnknownSection(ModuleDecoder* decoder,
                                             base::Vector<const uint8_t> bytes,
                                             uint32_t offset,
                                             SectionCode* result) {
  if (!decoder->ok()) return 0;
  decoder->impl_->Reset(bytes, offset);
  NoTracer no_tracer;
  *result = IdentifyUnknownSectionInternal(decoder->impl_.get(), no_tracer);
  return decoder->impl_->pc() - bytes.begin();
}

bool ModuleDecoder::ok() { return impl_->ok(); }

Result<const FunctionSig*> DecodeWasmSignatureForTesting(
    WasmFeatures enabled_features, Zone* zone,
    base::Vector<const uint8_t> bytes) {
  ModuleDecoderImpl decoder(enabled_features, bytes, kWasmOrigin);
  return decoder.toResult(decoder.DecodeFunctionSignature(zone, bytes.begin()));
}

ConstantExpression DecodeWasmInitExprForTesting(
    WasmFeatures enabled_features, base::Vector<const uint8_t> bytes,
    ValueType expected) {
  ModuleDecoderImpl decoder(enabled_features, bytes, kWasmOrigin);
  AccountingAllocator allocator;
  decoder.StartDecoding(nullptr, &allocator);
  return decoder.DecodeInitExprForTesting(expected);
}

FunctionResult DecodeWasmFunctionForTesting(
    WasmFeatures enabled_features, Zone* zone, ModuleWireBytes wire_bytes,
    const WasmModule* module, base::Vector<const uint8_t> function_bytes,
    Counters* counters) {
  if (function_bytes.size() > kV8MaxWasmFunctionSize) {
    return FunctionResult{
        WasmError{0, "size > maximum function size (%zu): %zu",
                  kV8MaxWasmFunctionSize, function_bytes.size()}};
  }
  ModuleDecoderImpl decoder(enabled_features, function_bytes, kWasmOrigin);
  decoder.SetCounters(counters);
  return decoder.DecodeSingleFunctionForTesting(zone, wire_bytes, module);
}

AsmJsOffsetsResult DecodeAsmJsOffsets(
    base::Vector<const uint8_t> encoded_offsets) {
  std::vector<AsmJsOffsetFunctionEntries> functions;

  Decoder decoder(encoded_offsets);
  uint32_t functions_count = decoder.consume_u32v("functions count");
  // Consistency check.
  DCHECK_GE(encoded_offsets.size(), functions_count);
  functions.reserve(functions_count);

  for (uint32_t i = 0; i < functions_count; ++i) {
    uint32_t size = decoder.consume_u32v("table size");
    if (size == 0) {
      functions.emplace_back();
      continue;
    }
    DCHECK(decoder.checkAvailable(size));
    const byte* table_end = decoder.pc() + size;
    uint32_t locals_size = decoder.consume_u32v("locals size");
    int function_start_position = decoder.consume_u32v("function start pos");
    int function_end_position = function_start_position;
    int last_byte_offset = locals_size;
    int last_asm_position = function_start_position;
    std::vector<AsmJsOffsetEntry> func_asm_offsets;
    func_asm_offsets.reserve(size / 4);  // conservative estimation
    // Add an entry for the stack check, associated with position 0.
    func_asm_offsets.push_back(
        {0, function_start_position, function_start_position});
    while (decoder.pc() < table_end) {
      DCHECK(decoder.ok());
      last_byte_offset += decoder.consume_u32v("byte offset delta");
      int call_position =
          last_asm_position + decoder.consume_i32v("call position delta");
      int to_number_position =
          call_position + decoder.consume_i32v("to_number position delta");
      last_asm_position = to_number_position;
      if (decoder.pc() == table_end) {
        // The last entry is the function end marker.
        DCHECK_EQ(call_position, to_number_position);
        function_end_position = call_position;
      } else {
        func_asm_offsets.push_back(
            {last_byte_offset, call_position, to_number_position});
      }
    }
    DCHECK_EQ(decoder.pc(), table_end);
    functions.emplace_back(AsmJsOffsetFunctionEntries{
        function_start_position, function_end_position,
        std::move(func_asm_offsets)});
  }
  DCHECK(decoder.ok());
  DCHECK(!decoder.more());

  return decoder.toResult(AsmJsOffsets{std::move(functions)});
}

std::vector<CustomSectionOffset> DecodeCustomSections(
    base::Vector<const uint8_t> bytes) {
  Decoder decoder(bytes);
  decoder.consume_bytes(4, "wasm magic");
  decoder.consume_bytes(4, "wasm version");

  std::vector<CustomSectionOffset> result;

  while (decoder.more()) {
    byte section_code = decoder.consume_u8("section code");
    uint32_t section_length = decoder.consume_u32v("section length");
    uint32_t section_start = decoder.pc_offset();
    if (section_code != 0) {
      // Skip known sections.
      decoder.consume_bytes(section_length, "section bytes");
      continue;
    }
    uint32_t name_length = decoder.consume_u32v("name length");
    uint32_t name_offset = decoder.pc_offset();
    decoder.consume_bytes(name_length, "section name");
    uint32_t payload_offset = decoder.pc_offset();
    if (section_length < (payload_offset - section_start)) {
      decoder.error("invalid section length");
      break;
    }
    uint32_t payload_length = section_length - (payload_offset - section_start);
    decoder.consume_bytes(payload_length);
    if (decoder.failed()) break;
    result.push_back({{section_start, section_length},
                      {name_offset, name_length},
                      {payload_offset, payload_length}});
  }

  return result;
}

namespace {

bool FindNameSection(Decoder* decoder) {
  static constexpr int kModuleHeaderSize = 8;
  decoder->consume_bytes(kModuleHeaderSize, "module header");

  NoTracer no_tracer;
  WasmSectionIterator section_iter(decoder, no_tracer);

  while (decoder->ok() && section_iter.more() &&
         section_iter.section_code() != kNameSectionCode) {
    section_iter.advance(true);
  }
  if (!section_iter.more()) return false;

  // Reset the decoder to not read beyond the name section end.
  decoder->Reset(section_iter.payload(), decoder->pc_offset());
  return true;
}

enum EmptyNames : bool { kAllowEmptyNames, kSkipEmptyNames };

void DecodeNameMap(NameMap& target, Decoder& decoder,
                   EmptyNames empty_names = kSkipEmptyNames) {
  uint32_t count = decoder.consume_u32v("names count");
  for (uint32_t i = 0; i < count; i++) {
    uint32_t index = decoder.consume_u32v("index");
    WireBytesRef name =
        consume_string(&decoder, unibrow::Utf8Variant::kLossyUtf8, "name");
    if (!decoder.ok()) break;
    if (index > NameMap::kMaxKey) continue;
    if (empty_names == kSkipEmptyNames && name.is_empty()) continue;
    if (!validate_utf8(&decoder, name)) continue;
    target.Put(index, name);
  }
  target.FinishInitialization();
}

void DecodeIndirectNameMap(IndirectNameMap& target, Decoder& decoder) {
  uint32_t outer_count = decoder.consume_u32v("outer count");
  for (uint32_t i = 0; i < outer_count; ++i) {
    uint32_t outer_index = decoder.consume_u32v("outer index");
    if (outer_index > IndirectNameMap::kMaxKey) continue;
    NameMap names;
    DecodeNameMap(names, decoder);
    target.Put(outer_index, std::move(names));
    if (!decoder.ok()) break;
  }
  target.FinishInitialization();
}

}  // namespace

void DecodeFunctionNames(base::Vector<const uint8_t> wire_bytes,
                         NameMap& names) {
  Decoder decoder(wire_bytes);
  if (FindNameSection(&decoder)) {
    while (decoder.ok() && decoder.more()) {
      uint8_t name_type = decoder.consume_u8("name type");
      if (name_type & 0x80) break;  // no varuint7

      uint32_t name_payload_len = decoder.consume_u32v("name payload length");
      if (!decoder.checkAvailable(name_payload_len)) break;

      if (name_type != NameSectionKindCode::kFunctionCode) {
        decoder.consume_bytes(name_payload_len, "name subsection payload");
        continue;
      }
      // We need to allow empty function names for spec-conformant stack traces.
      DecodeNameMap(names, decoder, kAllowEmptyNames);
      // The spec allows only one occurrence of each subsection. We could be
      // more permissive and allow repeated subsections; in that case we'd
      // have to delay calling {target.FinishInitialization()} on the function
      // names map until we've seen them all.
      // For now, we stop decoding after finding the first function names
      // subsection.
      return;
    }
  }
}

namespace {
// A task that validates multiple functions in parallel, storing the earliest
// validation error in {this} decoder.
class ValidateFunctionsTask : public JobTask {
 public:
  explicit ValidateFunctionsTask(base::Vector<const uint8_t> wire_bytes,
                                 const WasmModule* module,
                                 WasmFeatures enabled_features,
                                 std::function<bool(int)> filter,
                                 WasmError* error_out)
      : wire_bytes_(wire_bytes),
        module_(module),
        enabled_features_(enabled_features),
        filter_(std::move(filter)),
        next_function_(module->num_imported_functions),
        after_last_function_(next_function_ + module->num_declared_functions),
        error_out_(error_out) {
    DCHECK(error_out->empty());
  }

  void Run(JobDelegate* delegate) override {
    AccountingAllocator* allocator = module_->allocator();
    do {
      // Get the index of the next function to validate.
      // {fetch_add} might overrun {after_last_function_} by a bit. Since the
      // number of functions is limited to a value much smaller than the
      // integer range, this is near impossible to happen.
      static_assert(kV8MaxWasmFunctions < kMaxInt / 2);
      int func_index;
      do {
        func_index = next_function_.fetch_add(1, std::memory_order_relaxed);
        if (V8_UNLIKELY(func_index >= after_last_function_)) return;
        DCHECK_LE(0, func_index);
      } while (filter_ && !filter_(func_index));

      if (!ValidateFunction(allocator, func_index)) {
        // No need to validate any more functions.
        next_function_.store(after_last_function_, std::memory_order_relaxed);
        return;
      }
    } while (!delegate->ShouldYield());
  }

  size_t GetMaxConcurrency(size_t /* worker_count */) const override {
    int next_func = next_function_.load(std::memory_order_relaxed);
    return std::max(0, after_last_function_ - next_func);
  }

 private:
  bool ValidateFunction(AccountingAllocator* allocator, int func_index) {
    DCHECK(!module_->function_was_validated(func_index));
    WasmFeatures unused_detected_features;
    const WasmFunction& function = module_->functions[func_index];
    FunctionBody body{function.sig, function.code.offset(),
                      wire_bytes_.begin() + function.code.offset(),
                      wire_bytes_.begin() + function.code.end_offset()};
    DecodeResult validation_result = ValidateFunctionBody(
        allocator, enabled_features_, module_, &unused_detected_features, body);
    if (V8_UNLIKELY(validation_result.failed())) {
      SetError(func_index, std::move(validation_result).error());
      return false;
    }
    module_->set_function_validated(func_index);
    return true;
  }

  // Set the error from the argument if it's earlier than the error we already
  // have (or if we have none yet). Thread-safe.
  void SetError(int func_index, WasmError error) {
    base::MutexGuard mutex_guard{&set_error_mutex_};
    if (!error_out_->empty() && error_out_->offset() <= error.offset()) {
      return;
    }
    // Wrap the error message from the function decoder.
    const WasmFunction& function = module_->functions[func_index];
    WasmFunctionName func_name{
        &function,
        ModuleWireBytes{wire_bytes_}.GetNameOrNull(&function, module_)};
    std::ostringstream error_msg;
    error_msg << "in function " << func_name << ": " << error.message();
    *error_out_ = WasmError{error.offset(), error_msg.str()};
  }

  const base::Vector<const uint8_t> wire_bytes_;
  const WasmModule* const module_;
  const WasmFeatures enabled_features_;
  const std::function<bool(int)> filter_;
  std::atomic<int> next_function_;
  const int after_last_function_;
  base::Mutex set_error_mutex_;
  WasmError* const error_out_;
};
}  // namespace

WasmError ValidateFunctions(const WasmModule* module,
                            WasmFeatures enabled_features,
                            base::Vector<const uint8_t> wire_bytes,
                            std::function<bool(int)> filter) {
  DCHECK_EQ(kWasmOrigin, module->origin);

  class NeverYieldDelegate final : public JobDelegate {
   public:
    bool ShouldYield() override { return false; }

    bool IsJoiningThread() const override { UNIMPLEMENTED(); }
    void NotifyConcurrencyIncrease() override { UNIMPLEMENTED(); }
    uint8_t GetTaskId() override { UNIMPLEMENTED(); }
  };

  // Create a {ValidateFunctionsTask} to validate all functions. The earliest
  // error found will be set on this decoder.
  WasmError validation_error;
  std::unique_ptr<JobTask> validate_job =
      std::make_unique<ValidateFunctionsTask>(
          wire_bytes, module, enabled_features, std::move(filter),
          &validation_error);

  if (v8_flags.single_threaded) {
    // In single-threaded mode, run the {ValidateFunctionsTask} synchronously.
    NeverYieldDelegate delegate;
    validate_job->Run(&delegate);
  } else {
    // Spawn the task and join it.
    std::unique_ptr<JobHandle> job_handle = V8::GetCurrentPlatform()->CreateJob(
        TaskPriority::kUserVisible, std::move(validate_job));
    job_handle->Join();
  }

  return validation_error;
}

DecodedNameSection::DecodedNameSection(base::Vector<const uint8_t> wire_bytes,
                                       WireBytesRef name_section) {
  if (name_section.is_empty()) return;  // No name section.
  Decoder decoder(wire_bytes.begin() + name_section.offset(),
                  wire_bytes.begin() + name_section.end_offset(),
                  name_section.offset());
  while (decoder.ok() && decoder.more()) {
    uint8_t name_type = decoder.consume_u8("name type");
    if (name_type & 0x80) break;  // no varuint7

    uint32_t name_payload_len = decoder.consume_u32v("name payload length");
    if (!decoder.checkAvailable(name_payload_len)) break;

    switch (name_type) {
      case kModuleCode:
      case kFunctionCode:
        // Already handled elsewhere.
        decoder.consume_bytes(name_payload_len);
        break;
      case kLocalCode:
        if (local_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmFunctions <= IndirectNameMap::kMaxKey);
        static_assert(kV8MaxWasmFunctionLocals <= NameMap::kMaxKey);
        DecodeIndirectNameMap(local_names_, decoder);
        break;
      case kLabelCode:
        if (label_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmFunctions <= IndirectNameMap::kMaxKey);
        static_assert(kV8MaxWasmFunctionSize <= NameMap::kMaxKey);
        DecodeIndirectNameMap(label_names_, decoder);
        break;
      case kTypeCode:
        if (type_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmTypes <= NameMap::kMaxKey);
        DecodeNameMap(type_names_, decoder);
        break;
      case kTableCode:
        if (table_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmTables <= NameMap::kMaxKey);
        DecodeNameMap(table_names_, decoder);
        break;
      case kMemoryCode:
        if (memory_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmMemories <= NameMap::kMaxKey);
        DecodeNameMap(memory_names_, decoder);
        break;
      case kGlobalCode:
        if (global_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmGlobals <= NameMap::kMaxKey);
        DecodeNameMap(global_names_, decoder);
        break;
      case kElementSegmentCode:
        if (element_segment_names_.is_set()) {
          decoder.consume_bytes(name_payload_len);
        }
        static_assert(kV8MaxWasmTableInitEntries <= NameMap::kMaxKey);
        DecodeNameMap(element_segment_names_, decoder);
        break;
      case kDataSegmentCode:
        if (data_segment_names_.is_set()) {
          decoder.consume_bytes(name_payload_len);
        }
        static_assert(kV8MaxWasmDataSegments <= NameMap::kMaxKey);
        DecodeNameMap(data_segment_names_, decoder);
        break;
      case kFieldCode:
        if (field_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmTypes <= IndirectNameMap::kMaxKey);
        static_assert(kV8MaxWasmStructFields <= NameMap::kMaxKey);
        DecodeIndirectNameMap(field_names_, decoder);
        break;
      case kTagCode:
        if (tag_names_.is_set()) decoder.consume_bytes(name_payload_len);
        static_assert(kV8MaxWasmTags <= NameMap::kMaxKey);
        DecodeNameMap(tag_names_, decoder);
        break;
    }
  }
}

#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
