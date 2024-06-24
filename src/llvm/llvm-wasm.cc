#include "src/llvm/llvm-wasm.h"

using OptimizedCompilationJob = v8::internal::OptimizedCompilationJob;

#ifdef V8_ENABLE_LLVM

#include "src/llvm/wasm/wasm-compiler.h"

using Compiler = v8::internal::llvm::wasm::Compiler;

WasmCompilationResult v8::internal::llvm::wasm::ExecuteLLVMWasmCompilation(
    CompilationEnv* env, FunctionBody func_body) {
  return Compiler(env, std::move(func_body)).Compile();
}

std::unique_ptr<OptimizedCompilationJob>
v8::internal::llvm::wasm::NewJSToWasmCompilationJob() {
  UNREACHABLE();
}
#else
WasmCompilationResult v8::internal::llvm::wasm::ExecuteLLVMWasmCompilation(
    CompilationEnv* env, FunctionBody func_body) {
  UNREACHABLE();
}

std::unique_ptr<OptimizedCompilationJob>
v8::internal::llvm::wasm::NewJSToWasmCompilationJob() {
  UNREACHABLE();
}
#endif  // V8_ENABLE_LLVM