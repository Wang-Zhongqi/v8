#ifndef V8_LLVM_JS_COMPILER_H_
#define V8_LLVM_JS_COMPILER_H_

#include <memory>

#include "src/codegen/compiler.h"

namespace v8 {
namespace internal {
namespace llvm {
namespace js {
constexpr char compilerName[] = "LLVM";

class LLVMCompilationJob final : public OptimizedCompilationJob {
 public:
  static std::unique_ptr<LLVMCompilationJob> New(Isolate* isolate,
                                                 Handle<JSFunction> function,
                                                 BytecodeOffset osr_offset);

  explicit LLVMCompilationJob();

 private:
  Status PrepareJobImpl(Isolate* isolate) override;
  Status ExecuteJobImpl(RuntimeCallStats* stats,
                        LocalIsolate* local_heap) override;
  Status FinalizeJobImpl(Isolate* isolate) override;
};

}  // namespace js
}  // namespace llvm
}  // namespace internal
}  // namespace v8

#endif  // V8_LLVM_JS_COMPILER_H_