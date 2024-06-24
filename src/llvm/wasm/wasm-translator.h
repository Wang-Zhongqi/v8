#ifndef V8_LLVM_WASM_TRANSLATOR_H_
#define V8_LLVM_WASM_TRANSLATOR_H_

#include "llvm-c/Core.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace llvm {
namespace wasm {

using namespace internal::wasm;
using ValidationTag = Decoder::NoValidationTag;

class Translator {
 public:
  using FullDecoder = WasmFullDecoder<ValidationTag, Translator>;
  static constexpr bool kUsesPoppedArgs = true;
  Translator(LLVMContextRef context, LLVMModuleRef module);

  struct Value : public ValueBase<ValidationTag> {
    LLVMValueRef value = nullptr;

    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };

  struct Control : public ControlBase<Value, ValidationTag> {
    LLVMBasicBlockRef next = nullptr;
    union {
      LLVMBasicBlockRef elseBlock = nullptr;
      LLVMBasicBlockRef loopBlock;
    } block;

    FastZoneVector<LLVMValueRef> returnPhis;
    FastZoneVector<LLVMValueRef> loopPhis;

    Zone* zone;

    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Control);

    template <typename... Args>
    explicit Control(Zone* zone, Args&&... args) V8_NOEXCEPT
        : ControlBase(zone, std::forward<Args>(args)...), zone(zone) {}

    ~Control() {
      this->returnPhis.Reset(zone);
      this->loopPhis.Reset(zone);
    }
  };

  void StartFunction(FullDecoder* decoder);

  void StartFunctionBody(FullDecoder* decoder, Control* block);

  void FinishFunction(FullDecoder* decoder);

  void OnFirstError(FullDecoder*) {}

  void NextInstruction(FullDecoder*, WasmOpcode) {}

  void Block(FullDecoder* decoder, Control* block) {}

  void Loop(FullDecoder* decoder, Control* block) {}

  void Try(FullDecoder* decoder, Control* block) {}

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {}

  void FallThruTo(FullDecoder* decoder, Control* c) {}

  void PopControl(FullDecoder* decoder, Control* block);

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {}

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result);

  void TraceInstruction(FullDecoder* decoder, uint32_t markid) {}

  void I32Const(FullDecoder* decoder, Value* result, int32_t value);

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {}

  void F32Const(FullDecoder* decoder, Value* result, float value) {}

  void F64Const(FullDecoder* decoder, Value* result, double value) {}

  void S128Const(FullDecoder* decoder, const Simd128Immediate& imm,
                 Value* result) {}

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {}

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {}

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {}

  void Drop(FullDecoder* decoder) {}

  void LocalGet(FullDecoder* decoder, Value* result, const IndexImmediate& imm);

  void LocalSet(FullDecoder* decoder, const Value& value,
                const IndexImmediate& imm) {}

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const IndexImmediate& imm) {}

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate& imm) {}

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate& imm) {}

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const IndexImmediate& imm) {}

  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const IndexImmediate& imm) {}

  void Trap(FullDecoder* decoder, TrapReason reason) {}

  void AssertNullTypecheck(FullDecoder* decoder, const Value& obj,
                           Value* result) {}

  void AssertNotNullTypecheck(FullDecoder* decoder, const Value& obj,
                              Value* result) {}

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {}

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {}

  void DoReturn(FullDecoder* decoder, uint32_t drop_values);

  void BrOrRet(FullDecoder* decoder, uint32_t depth, uint32_t drop_values = 0) {
  }

  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {}

  void BrTable(FullDecoder* decoder, const BranchTableImmediate& imm,
               const Value& key) {}

  void Else(FullDecoder* decoder, Control* if_block) {}

  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate& imm, const Value& index,
               Value* result) {}

  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate& imm, const Value& index,
                     Value* result) {}

  void LoadLane(FullDecoder* decoder, LoadType type, const Value& value,
                const Value& index, const MemoryAccessImmediate& imm,
                const uint8_t laneidx, Value* result) {}

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate& imm, const Value& index,
                const Value& value) {}

  void StoreLane(FullDecoder* decoder, StoreType type,
                 const MemoryAccessImmediate& imm, const Value& index,
                 const Value& value, const uint8_t laneidx) {}

  void CurrentMemoryPages(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                          Value* result) {}

  void MemoryGrow(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& value, Value* result) {}

  bool HandleWellKnownImport(FullDecoder* decoder, uint32_t index,
                             const Value args[], Value returns[]) {
    UNREACHABLE();
  }

  void CallDirect(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[], Value returns[]) {}

  void ReturnCall(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[]) {}

  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate& imm, const Value args[],
                    Value returns[]) {}

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate& imm,
                          const Value args[]) {}

  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const FunctionSig* sig, const Value args[], Value returns[]) {}

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const FunctionSig* sig, const Value args[]) {}

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth,
                bool pass_null_along_branch, Value* result_on_fallthrough) {}

  void BrOnNonNull(FullDecoder* decoder, const Value& ref_object, Value* result,
                   uint32_t depth, bool /* drop_null_on_fallthrough */) {}

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, const Value* args,
              Value* result) {}

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate& imm,
                  base::Vector<const Value> inputs, Value* result) {}

  void Simd8x16ShuffleOp(FullDecoder* decoder, const Simd128Immediate& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {}

  void Throw(FullDecoder* decoder, const TagIndexImmediate& imm,
             const Value arg_values[]) {}

  void Rethrow(FullDecoder* decoder, Control* block) {}

  void CatchException(FullDecoder* decoder, const TagIndexImmediate& imm,
                      Control* block, base::Vector<Value> values) {}

  void Delegate(FullDecoder* decoder, uint32_t depth, Control* block) {}

  void CatchAll(FullDecoder* decoder, Control* block) {}

  void TryTable(FullDecoder* decoder, Control* block) {}

  void CatchCase(FullDecoder* decoder, Control* block,
                 const CatchCase& catch_case, base::Vector<Value> values) {}

  void ThrowRef(FullDecoder* decoder, Value* value) {}

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, const Value args[],
                const size_t argc, const MemoryAccessImmediate& imm,
                Value* result) {}

  void AtomicFence(FullDecoder* decoder) {}

  void MemoryInit(FullDecoder* decoder, const MemoryInitImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {}

  void DataDrop(FullDecoder* decoder, const IndexImmediate& imm) {}

  void MemoryCopy(FullDecoder* decoder, const MemoryCopyImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {}

  void MemoryFill(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& dst, const Value& value, const Value& size) {}

  void TableInit(FullDecoder* decoder, const TableInitImmediate& imm,
                 const Value* args) {}

  void ElemDrop(FullDecoder* decoder, const IndexImmediate& imm) {}

  void TableCopy(FullDecoder* decoder, const TableCopyImmediate& imm,
                 const Value args[]) {}

  void TableGrow(FullDecoder* decoder, const IndexImmediate& imm,
                 const Value& value, const Value& delta, Value* result) {}

  void TableSize(FullDecoder* decoder, const IndexImmediate& imm,
                 Value* result) {}

  void TableFill(FullDecoder* decoder, const IndexImmediate& imm,
                 const Value& start, const Value& value, const Value& count) {}

  void StructNew(FullDecoder* decoder, const StructIndexImmediate& imm,
                 const Value args[], Value* result) {}

  void StructNewDefault(FullDecoder* decoder, const StructIndexImmediate& imm,
                        Value* result) {}

  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, bool is_signed, Value* result) {}

  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, const Value& field_value) {}

  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                const Value& length, const Value& initial_value,
                Value* result) {}

  void ArrayNewDefault(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                       const Value& length, Value* result) {}

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                bool is_signed, Value* result) {}

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                const Value& value) {}

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {}

  void ArrayCopy(FullDecoder* decoder, const Value& dst, const Value& dst_index,
                 const Value& src, const Value& src_index,
                 const ArrayIndexImmediate& src_imm, const Value& length) {}

  void ArrayFill(FullDecoder* decoder, ArrayIndexImmediate& imm,
                 const Value& array, const Value& index, const Value& value,
                 const Value& length) {}

  void ArrayNewFixed(FullDecoder* decoder, const ArrayIndexImmediate& array_imm,
                     const IndexImmediate& length_imm, const Value elements[],
                     Value* result) {}

  void ArrayNewSegment(FullDecoder* decoder,
                       const ArrayIndexImmediate& array_imm,
                       const IndexImmediate& segment_imm, const Value& offset,
                       const Value& length, Value* result) {}

  void ArrayInitSegment(FullDecoder* decoder,
                        const ArrayIndexImmediate& array_imm,
                        const IndexImmediate& segment_imm, const Value& array,
                        const Value& array_index, const Value& segment_offset,
                        const Value& length) {}

  void RefI31(FullDecoder* decoder, const Value& input, Value* result) {}

  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {}

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {}

  void RefTest(FullDecoder* decoder, uint32_t ref_index, const Value& object,
               Value* result, bool null_succeeds) {}

  void RefTestAbstract(FullDecoder* decoder, const Value& object,
                       wasm::HeapType type, Value* result, bool null_succeeds) {
  }

  void RefCast(FullDecoder* decoder, uint32_t ref_index, const Value& object,
               Value* result, bool null_succeeds) {}

  void RefCastAbstract(FullDecoder* decoder, const Value& object,
                       wasm::HeapType type, Value* result, bool null_succeeds) {
  }

  void BrOnCast(FullDecoder* decoder, uint32_t ref_index, const Value& object,
                Value* value_on_branch, uint32_t br_depth, bool null_succeeds) {
  }

  void BrOnCastFail(FullDecoder* decoder, uint32_t ref_index,
                    const Value& object, Value* value_on_fallthrough,
                    uint32_t br_depth, bool null_succeeds) {}

  void BrOnCastAbstract(FullDecoder* decoder, const Value& object,
                        HeapType type, Value* value_on_branch,
                        uint32_t br_depth, bool null_succeeds) {}

  void BrOnCastFailAbstract(FullDecoder* decoder, const Value& object,
                            HeapType type, Value* value_on_fallthrough,
                            uint32_t br_depth, bool null_succeeds) {}

  void BrOnEq(FullDecoder* decoder, const Value& object, Value* value_on_branch,
              uint32_t br_depth, bool null_succeeds) {}

  void BrOnNonEq(FullDecoder* decoder, const Value& object,
                 Value* value_on_fallthrough, uint32_t br_depth,
                 bool null_succeeds) {}

  void BrOnStruct(FullDecoder* decoder, const Value& object,
                  Value* value_on_branch, uint32_t br_depth,
                  bool null_succeeds) {}

  void BrOnNonStruct(FullDecoder* decoder, const Value& object,
                     Value* value_on_fallthrough, uint32_t br_depth,
                     bool null_succeeds) {}

  void BrOnArray(FullDecoder* decoder, const Value& object,
                 Value* value_on_branch, uint32_t br_depth,
                 bool null_succeeds) {}

  void BrOnNonArray(FullDecoder* decoder, const Value& object,
                    Value* value_on_fallthrough, uint32_t br_depth,
                    bool null_succeeds) {}

  void BrOnI31(FullDecoder* decoder, const Value& object,
               Value* value_on_branch, uint32_t br_depth, bool null_succeeds) {}

  void BrOnNonI31(FullDecoder* decoder, const Value& object,
                  Value* value_on_fallthrough, uint32_t br_depth,
                  bool null_succeeds) {}

  void BrOnString(FullDecoder* decoder, const Value& object,
                  Value* value_on_branch, uint32_t br_depth,
                  bool null_succeeds) {}

  void BrOnNonString(FullDecoder* decoder, const Value& object,
                     Value* value_on_fallthrough, uint32_t br_depth,
                     bool null_succeeds) {}

  void StringNewWtf8(FullDecoder* decoder, const MemoryIndexImmediate& memory,
                     const unibrow::Utf8Variant variant, const Value& offset,
                     const Value& size, Value* result) {}

  void StringNewWtf8Array(FullDecoder* decoder,
                          const unibrow::Utf8Variant variant,
                          const Value& array, const Value& start,
                          const Value& end, Value* result) {}

  void StringNewWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                      const Value& offset, const Value& size, Value* result) {}

  void StringNewWtf16Array(FullDecoder* decoder, const Value& array,
                           const Value& start, const Value& end,
                           Value* result) {}

  void StringConst(FullDecoder* decoder, const StringConstImmediate& imm,
                   Value* result) {}

  void StringMeasureWtf8(FullDecoder* decoder,
                         const unibrow::Utf8Variant variant, const Value& str,
                         Value* result) {}

  void StringMeasureWtf16(FullDecoder* decoder, const Value& str,
                          Value* result) {}

  void StringEncodeWtf8(FullDecoder* decoder,
                        const MemoryIndexImmediate& memory,
                        const unibrow::Utf8Variant variant, const Value& str,
                        const Value& offset, Value* result) {}

  void StringEncodeWtf8Array(FullDecoder* decoder,
                             const unibrow::Utf8Variant variant,
                             const Value& str, const Value& array,
                             const Value& start, Value* result) {}
  void StringEncodeWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                         const Value& str, const Value& offset, Value* result) {
  }

  void StringEncodeWtf16Array(FullDecoder* decoder, const Value& str,
                              const Value& array, const Value& start,
                              Value* result) {}

  void StringConcat(FullDecoder* decoder, const Value& head, const Value& tail,
                    Value* result) {}

  void StringEq(FullDecoder* decoder, const Value& a, const Value& b,
                Value* result) {}

  void StringIsUSVSequence(FullDecoder* decoder, const Value& str,
                           Value* result) {}

  void StringAsWtf8(FullDecoder* decoder, const Value& str, Value* result) {}

  void StringViewWtf8Advance(FullDecoder* decoder, const Value& view,
                             const Value& pos, const Value& bytes,
                             Value* result) {}

  void StringViewWtf8Encode(FullDecoder* decoder,
                            const MemoryIndexImmediate& memory,
                            const unibrow::Utf8Variant variant,
                            const Value& view, const Value& addr,
                            const Value& pos, const Value& bytes,
                            Value* next_pos, Value* bytes_written) {}

  void StringViewWtf8Slice(FullDecoder* decoder, const Value& view,
                           const Value& start, const Value& end,
                           Value* result) {}

  void StringAsWtf16(FullDecoder* decoder, const Value& str, Value* result) {}

  void StringViewWtf16GetCodeUnit(FullDecoder* decoder, const Value& view,
                                  const Value& pos, Value* result) {}

  void StringViewWtf16Encode(FullDecoder* decoder,
                             const MemoryIndexImmediate& imm, const Value& view,
                             const Value& offset, const Value& pos,
                             const Value& codeunits, Value* result) {}

  void StringViewWtf16Slice(FullDecoder* decoder, const Value& view,
                            const Value& start, const Value& end,
                            Value* result) {}

  void StringAsIter(FullDecoder* decoder, const Value& str, Value* result) {}

  void StringViewIterNext(FullDecoder* decoder, const Value& view,
                          Value* result) {}

  void StringViewIterAdvance(FullDecoder* decoder, const Value& view,
                             const Value& codepoints, Value* result) {}

  void StringViewIterRewind(FullDecoder* decoder, const Value& view,
                            const Value& codepoints, Value* result) {}

  void StringViewIterSlice(FullDecoder* decoder, const Value& view,
                           const Value& codepoints, Value* result) {}

  void StringCompare(FullDecoder* decoder, const Value& lhs, const Value& rhs,
                     Value* result) {}

  void StringFromCodePoint(FullDecoder* decoder, const Value& code_point,
                           Value* result) {}

  void StringHash(FullDecoder* decoder, const Value& string, Value* result) {}

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {}

  bool emit_loop_exits() { UNREACHABLE(); }

  bool inlining_enabled(FullDecoder* decoder) { UNREACHABLE(); }

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge,
                       Value* values) {}

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge,
                       uint32_t drop_values = 0) {}

  const CallSiteFeedback& next_call_feedback() { UNREACHABLE(); }

  void BuildLoopExits(FullDecoder* decoder, Control* loop) {}

  void WrapLocalsAtLoopExit(FullDecoder* decoder, Control* loop) {}

  int FindFirstUsedMemoryIndex(base::Vector<const uint8_t> body, Zone* zone) {
    UNREACHABLE();
  }

 private:
  LLVMContextRef context;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMValueRef func;

  FastZoneVector<LLVMValueRef> locals;


  LLVMTypeRef TypeToLLVM(const ValueType& type);
  LLVMTypeRef GetReturenType(const FunctionSig* sig);
  FastZoneVector<LLVMTypeRef> GetParamType(FullDecoder* decoder);  
  void BuildLocals(FullDecoder* decoder);
  void BuildReturns(FullDecoder* decoder, Control*);
};

}  // namespace wasm
}  // namespace llvm
}  // namespace internal
}  // namespace v8

#endif  // V8_LLVM_WASM_TRANSLATOR_H_