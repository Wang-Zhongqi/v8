#ifndef V8_LLVM_WASM_COMPILER_H_
#define V8_LLVM_WASM_COMPILER_H_

#include "llvm-c/Core.h"
#include "llvm-c/TargetMachine.h"
#include "src/wasm/function-compiler.h"

namespace v8 {
namespace internal {
namespace llvm {
namespace wasm {

using WasmCompilationResult = internal::wasm::WasmCompilationResult;
using CompilationEnv = internal::wasm::CompilationEnv;
using FunctionBody = internal::wasm::FunctionBody;

class Compiler {
 public:
  Compiler(CompilationEnv* env, FunctionBody func_body);
  WasmCompilationResult Compile();

 private:
  void Translate();
  void Optimise();
  WasmCompilationResult Emit();

  LLVMTargetMachineRef targetMachine;
  LLVMContextRef context;
  LLVMModuleRef module;

  CompilationEnv* env;
  FunctionBody func_body;
};

}  // namespace wasm
}  // namespace llvm
}  // namespace internal
}  // namespace v8

#endif  // V8_LLVM_WASM_COMPILER_H_