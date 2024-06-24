#ifndef V8_LLVM_WASM_H_
#define V8_LLVM_WASM_H_

#include <memory>

#include "src/wasm/function-compiler.h"

using WasmCompilationResult = v8::internal::wasm::WasmCompilationResult;
using CompilationEnv = v8::internal::wasm::CompilationEnv;
using FunctionBody = v8::internal::wasm::FunctionBody;

namespace v8 {
namespace internal {
namespace llvm {
namespace wasm {

WasmCompilationResult ExecuteLLVMWasmCompilation(CompilationEnv* env,
                                                 FunctionBody func_body);

std::unique_ptr<OptimizedCompilationJob> NewJSToWasmCompilationJob();

}  // namespace wasm
}  // namespace llvm
}  // namespace internal
}  // namespace v8

#endif  // V8_LLVM_WASM_H_