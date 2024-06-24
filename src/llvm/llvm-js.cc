#include "src/llvm/llvm-js.h"

using namespace v8::internal;

#ifdef V8_ENABLE_LLVM

#include "src/llvm/js/js-compiler.h"

using LLVMCompilationJob = v8::internal::llvm::js::LLVMCompilationJob;

MaybeHandle<Code> v8::internal::llvm::js::CompileLLVM(
    Isolate* isolate, Handle<JSFunction> function, BytecodeOffset osr_offset) {
  DCHECK(llvm::IsLLVMEnabled());

  auto job = LLVMCompilationJob::New(isolate, function, osr_offset);
  UNREACHABLE();
}
#else
MaybeHandle<Code> v8::internal::llvm::js::CompileLLVM(
    Isolate* isolate, Handle<JSFunction> function, BytecodeOffset osr_offset) {
  UNREACHABLE();
}
#endif  // V8_ENABLE_LLVM