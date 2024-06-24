#include "src/llvm/wasm/wasm-translator.h"

#include "llvm-c/Analysis.h"

namespace v8 {
namespace internal {
namespace llvm {
namespace wasm {

using FunctionSig = internal::wasm::FunctionSig;
using ValueKind = internal::wasm::ValueKind;

Translator::Translator(LLVMContextRef context, LLVMModuleRef module)
    : context(context), module(module) {
  this->builder = LLVMCreateBuilderInContext(this->context);
}

LLVMTypeRef Translator::TypeToLLVM(const ValueType& type) {
  switch (type.kind()) {
    case ValueKind::kVoid:
      return LLVMVoidTypeInContext(this->context);
    case ValueKind::kI32:
      return LLVMInt32TypeInContext(this->context);
    case ValueKind::kI64:
      return LLVMInt64TypeInContext(this->context);
    case ValueKind::kF32:
      return LLVMFloatTypeInContext(this->context);
    case ValueKind::kF64:
      return LLVMDoubleTypeInContext(this->context);
    default:
      UNREACHABLE();
  }
}

LLVMTypeRef Translator::GetReturenType(const FunctionSig* sig) {
  switch (sig->return_count()) {
    case 0:
      return LLVMVoidTypeInContext(this->context);
    case 1:
      return this->TypeToLLVM(sig->GetReturn());
    default:
      UNREACHABLE();
  }
}

FastZoneVector<LLVMTypeRef> Translator::GetParamType(FullDecoder* decoder) {
  const FunctionSig* sig = decoder->sig_;
  // FastZoneVector<LLVMTypeRef> params(1 + (int)sig->parameter_count(),
  //                                    decoder->zone());
  // params.emplace_back(LLVMPointerType(LLVMInt8TypeInContext(this->context), 0));
  FastZoneVector<LLVMTypeRef> params((int)sig->parameter_count(),
                                   decoder->zone());
  for (const ValueType& type : sig->parameters()) {
    params.emplace_back(this->TypeToLLVM(type));
  }
  return params;
}

void Translator::StartFunction(FullDecoder* decoder) {
  LLVMTypeRef returnType = GetReturenType(decoder->sig_);
  FastZoneVector<LLVMTypeRef> params = GetParamType(decoder);
  LLVMTypeRef funcType = LLVMFunctionType(returnType, params.begin(),
                                          (unsigned)params.size(), false);
  this->func = LLVMAddFunction(this->module, "func", funcType);

  // LLVMSetLinkage(this->func, LLVMExternalLinkage);

  // LLVMSetLinkage(this->func, LLVMDLLExportLinkage);
  // LLVMSetDLLStorageClass(this->func, LLVMDLLExportStorageClass);

  params.Reset(decoder->zone());
}

void Translator::BuildLocals(FullDecoder* decoder) {
  LLVMBasicBlockRef entryBlock =
      LLVMAppendBasicBlockInContext(this->context, func, "entry_block");
  LLVMPositionBuilderAtEnd(this->builder, entryBlock);

  this->locals.EnsureMoreCapacity(decoder->num_locals(), decoder->zone());
  for (u_int32_t index = 0; index < decoder->num_locals(); index++) {
    LLVMTypeRef llvmType = this->TypeToLLVM(decoder->local_type(index));
    LLVMValueRef llvmLocal = LLVMBuildAlloca(this->builder, llvmType, "local");
    if (index < decoder->sig_->parameter_count()) {
      // LLVMBuildStore(this->builder, LLVMGetParam(this->func, index + 1), llvmLocal);
      LLVMBuildStore(this->builder, LLVMGetParam(this->func, index), llvmLocal);
    } else {
      LLVMBuildStore(this->builder, LLVMConstNull(llvmType), llvmLocal);
    }
    this->locals.emplace_back(llvmLocal);
  }
  LLVMBasicBlockRef startBlock =
      LLVMAppendBasicBlockInContext(this->context, this->func, "start_block");
  LLVMBuildBr(this->builder, startBlock);
  LLVMPositionBuilderAtEnd(this->builder, startBlock);
}

void Translator::BuildReturns(FullDecoder* decoder, Control* block) {
  LLVMBasicBlockRef startBlock = LLVMGetInsertBlock(this->builder);
  block->next =
      LLVMAppendBasicBlockInContext(this->context, func, "return_block");
  LLVMPositionBuilderAtEnd(this->builder, block->next);

  const FunctionSig* sig = decoder->sig_;
  block->returnPhis.EnsureMoreCapacity((int)sig->return_count(),
                                       decoder->zone());
  for (const ValueType& type : sig->returns()) {
    block->returnPhis.emplace_back(
        LLVMBuildPhi(this->builder, this->TypeToLLVM(type), ""));
  }

  switch (sig->return_count()) {
    case 0:
      LLVMBuildRetVoid(this->builder);
      break;
    case 1:
      LLVMBuildRet(this->builder, block->returnPhis[0]);
      break;
    default:
      UNREACHABLE();
  }

  LLVMPositionBuilderAtEnd(this->builder, startBlock);
}

void Translator::StartFunctionBody(FullDecoder* decoder, Control* block) {
  BuildLocals(decoder);
  BuildReturns(decoder, block);
}

void Translator::FinishFunction(FullDecoder* decoder) {
  this->locals.Reset(decoder->zone());

  char* errMessage = nullptr;

  if (LLVMVerifyModule(this->module, LLVMAbortProcessAction, &errMessage)) {
    printf("%s\n", errMessage);
    UNREACHABLE();
  }
}

void Translator::PopControl(FullDecoder* decoder, Control* block) {
  LLVMBasicBlockRef curentBlock = LLVMGetInsertBlock(this->builder);
  FastZoneVector<LLVMValueRef>& phis = block->returnPhis;
  for (uint32_t i = 0; i < phis.size(); ++i) {
    LLVMValueRef value = decoder->stack_value(i + 1)->value;
    LLVMAddIncoming(phis[i], &value, &curentBlock, 1);
  }

  LLVMBuildBr(this->builder, block->next);
  LLVMPositionBuilderAtEnd(this->builder, block->next);
}

void Translator::BinOp(FullDecoder* decoder, WasmOpcode opcode,
                       const Value& lhs, const Value& rhs, Value* result) {
  switch (opcode) {
    case kExprI32Add:
      result->value = LLVMBuildAdd(this->builder, lhs.value, rhs.value, "add");
      break;
    case kExprI32Mul:
      result->value = LLVMBuildMul(this->builder, lhs.value, rhs.value, "mul");
      break;
    default:
      UNREACHABLE();
  }
}

void Translator::I32Const(FullDecoder* decoder, Value* result, int32_t value) {
  result->value = LLVMConstInt(LLVMInt32TypeInContext(this->context), value, false);
}

void Translator::LocalGet(FullDecoder* decoder, Value* result,
                          const IndexImmediate& imm) {
  LLVMTypeRef type = this->TypeToLLVM(result->type);
  result->value =
      LLVMBuildLoad2(this->builder, type, this->locals[imm.index], "local_get");
}

void Translator::DoReturn(FullDecoder* decoder, uint32_t drop_values) {
  if (decoder->control_depth() == 1) {
    PopControl(decoder, decoder->control_at(0));
  }
}

}  // namespace wasm
}  // namespace llvm
}  // namespace internal
}  // namespace v8
