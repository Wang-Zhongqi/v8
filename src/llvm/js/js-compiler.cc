#include "src/llvm/js/js-compiler.h"

#include "llvm-c/TargetMachine.h"

namespace v8 {
namespace internal {

using Status = CompilationJob::Status;

namespace llvm {
namespace js {

std::unique_ptr<LLVMCompilationJob> LLVMCompilationJob::New(
    Isolate* isolate, Handle<JSFunction> function, BytecodeOffset osr_offset) {
  char* triple = LLVMGetDefaultTargetTriple();
  char cpu[] = "generic";
  char features[] = "";
  char* errMessage = nullptr;

  LLVMTargetRef target;
  LLVMGetTargetFromTriple(triple, &target, &errMessage);

  // LLVMTargetMachineRef targetMachine = LLVMCreateTargetMachine(target,
  // triple, cpu, features, LLVMCodeGenLevelAggressive, LLVMRelocDefault,
  // LLVMCodeModelSmall);
  LLVMCreateTargetMachine(target, triple, cpu, features,
                          LLVMCodeGenLevelAggressive, LLVMRelocDefault,
                          LLVMCodeModelSmall);

  return std::make_unique<LLVMCompilationJob>();
}

LLVMCompilationJob::LLVMCompilationJob()
    : OptimizedCompilationJob(compilerName, State::kReadyToPrepare) {
  printf("LLVMCompilationInfo\n");
}

Status LLVMCompilationJob::PrepareJobImpl(Isolate* isolate) { UNREACHABLE(); }
Status LLVMCompilationJob::ExecuteJobImpl(RuntimeCallStats* stats,
                                          LocalIsolate* local_heap) {
  UNREACHABLE();
}
Status LLVMCompilationJob::FinalizeJobImpl(Isolate* isolate) { UNREACHABLE(); }

}  // namespace js
}  // namespace llvm
}  // namespace internal
}  // namespace v8