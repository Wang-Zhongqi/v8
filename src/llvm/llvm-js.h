#ifndef V8_LLVM_JS_H_
#define V8_LLVM_JS_H_

#include <memory>

#include "src/codegen/compiler.h"

namespace v8 {
namespace internal {
namespace llvm {
namespace js {

MaybeHandle<Code> CompileLLVM(Isolate* isolate, Handle<JSFunction> function,
                              BytecodeOffset osr_offset);

}  // namespace js
}  // namespace llvm
}  // namespace internal
}  // namespace v8

#endif  // V8_LLVM_JS_H_