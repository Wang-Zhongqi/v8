#include "src/llvm/wasm/wasm-compiler.h"

#include "assert.h"
#include "llvm-c/Object.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "src/llvm/wasm/wasm-translator.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace llvm {
namespace wasm {

using ValidationTag = internal::wasm::Decoder::NoValidationTag;
using FullDecoder = internal::wasm::WasmFullDecoder<ValidationTag, Translator>;
using WasmFeatures = internal::wasm::WasmFeatures;

Compiler::Compiler(CompilationEnv* env, FunctionBody func_body)
    : env(env), func_body(func_body) {
  char* triple = LLVMGetDefaultTargetTriple();
  char features[] = "";
  char* errMessage = nullptr;
  char module_name[] = "";

  if (strncmp(triple, "x86_64", 6) == 0) {
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();
    LLVMInitializeX86Disassembler();
    LLVMInitializeX86TargetMC();
  } else if (strncmp(triple, "Aarch64", 7) == 0) {
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();
    LLVMInitializeAArch64Disassembler();
    LLVMInitializeAArch64TargetMC();

  } else {
    printf("Error: %s is not supported.\n", triple);
    UNREACHABLE();
  }

  LLVMTargetRef target = nullptr;
  LLVMGetTargetFromTriple(triple, &target, &errMessage);

  if (errMessage != nullptr) {
    printf("%s\n", errMessage);
    UNREACHABLE();
  }

  this->targetMachine = LLVMCreateTargetMachine(
      target, triple, "generic", features, LLVMCodeGenLevelAggressive,
      LLVMRelocDefault, LLVMCodeModelSmall);

  this->context = LLVMContextCreate();
  this->module = LLVMModuleCreateWithNameInContext(module_name, this->context);
}

WasmCompilationResult Compiler::Compile() {
  Translate();
  Optimise();
  return Emit();
}

void Compiler::Translate() {
  Zone zone(internal::wasm::GetWasmEngine()->allocator(), ZONE_NAME);
  WasmFeatures detached = WasmFeatures::All();
  FullDecoder(&zone, this->env->module, this->env->enabled_features, &detached,
              this->func_body, this->context, this->module)
      .Decode();
}

void Compiler::Optimise() {
  char* errMessage = nullptr;
  LLVMPrintModuleToFile(this->module, "test.preopt.ll", &errMessage);
  if (errMessage != nullptr) {
    UNREACHABLE();
  }
  LLVMErrorRef error =
      LLVMRunPasses(this->module, "default<O3>", this->targetMachine,
                    LLVMCreatePassBuilderOptions());
  if (error != nullptr) {
    UNREACHABLE();
  }
  LLVMPrintModuleToFile(this->module, "test.postopt.ll", &errMessage);
  if (errMessage != nullptr) {
    UNREACHABLE();
  }
}

class V8LLVMAssemblerBuffer : public AssemblerBuffer {
 public:
  explicit V8LLVMAssemblerBuffer(int size)
      : buffer(base::OwnedVector<uint8_t>::NewForOverwrite(
            std::max(AssemblerBase::kMinimalBufferSize, size))) {}

  uint8_t* start() const override { return buffer.begin(); }

  int size() const override { return static_cast<int>(buffer.size()); }

  std::unique_ptr<AssemblerBuffer> Grow(int new_size) override {
    UNREACHABLE();
  }

  base::OwnedVector<uint8_t> buffer;
};

std::pair<std::unique_ptr<V8LLVMAssemblerBuffer>, CodeDesc> LLVMObjToV8Buffer(
    LLVMBinaryRef obj) {
  CodeDesc codeDesc;

  const char* text = nullptr;
  const char* rodata = nullptr;

  LLVMSectionIteratorRef iter = LLVMObjectFileCopySectionIterator(obj);
  while (!LLVMObjectFileIsSectionIteratorAtEnd(obj, iter)) {
    const char* symbolName = LLVMGetSectionName(iter);
    const char* content = LLVMGetSectionContents(iter);

    if (symbolName == nullptr) {
      DCHECK(strcmp(content, "\177ELF\2\1\1") == 0);
    } else if (strcmp(symbolName, ".text") == 0) {
      codeDesc.safepoint_table_offset = (int32_t)LLVMGetSectionSize(iter);
      codeDesc.instr_size += codeDesc.safepoint_table_offset;
      text = LLVMGetSectionContents(iter);
    } else if (strcmp(symbolName, ".rodata") == 0) {
      codeDesc.constant_pool_size = (int32_t)LLVMGetSectionSize(iter);
      codeDesc.instr_size += codeDesc.constant_pool_size;
      rodata = LLVMGetSectionContents(iter);
    } else {
      printf("%s\n", symbolName);
      printf("%s\n", content);
    }

    LLVMMoveToNextSection(iter);
  }

  codeDesc.handler_table_offset =
      codeDesc.safepoint_table_offset + codeDesc.safepoint_table_size;
  codeDesc.constant_pool_offset =
      codeDesc.handler_table_offset + codeDesc.handler_table_size;
  codeDesc.code_comments_offset =
      codeDesc.constant_pool_offset + codeDesc.constant_pool_size;

  // todo: reloc_offset calc
  codeDesc.reloc_offset =
      codeDesc.code_comments_offset + codeDesc.code_comments_size;
  codeDesc.buffer_size = codeDesc.reloc_offset + codeDesc.reloc_size;

  std::unique_ptr<V8LLVMAssemblerBuffer> buffer =
      std::make_unique<V8LLVMAssemblerBuffer>(codeDesc.buffer_size);
  codeDesc.buffer = buffer->buffer.data();
  memcpy(codeDesc.buffer, text, codeDesc.instr_size);
  memcpy(codeDesc.buffer + codeDesc.reloc_offset, rodata,
         codeDesc.constant_pool_size);

  // todo: set content

  return std::make_pair(std::move(buffer), codeDesc);
}

WasmCompilationResult Compiler::Emit() {
  char* errMessage = nullptr;
  LLVMMemoryBufferRef buffer = nullptr;
  LLVMTargetMachineEmitToMemoryBuffer(this->targetMachine, this->module,
                                      LLVMObjectFile, &errMessage, &buffer);

  LLVMTargetMachineEmitToFile(this->targetMachine, this->module, "test.a",
                              LLVMObjectFile, &errMessage);

  if (errMessage != nullptr) {
    printf("%s\n", errMessage);
    UNREACHABLE();
  }

  LLVMBinaryRef obj = LLVMCreateBinary(buffer, this->context, &errMessage);
  if (errMessage != nullptr) {
    printf("%s\n", errMessage);
    UNREACHABLE();
  }

  WasmCompilationResult result;
  std::tie(result.instr_buffer, result.code_desc) = LLVMObjToV8Buffer(obj);
  result.result_tier = ExecutionTier::kLLVM;
  return result;
}

}  // namespace wasm
}  // namespace llvm
}  // namespace internal
}  // namespace v8
