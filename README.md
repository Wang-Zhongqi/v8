# llv8调研
## 相关资料
**学习视频**：[深入v8引擎](https://space.bilibili.com/296494084/search/video?keyword=%E6%B7%B1%E5%85%A5v8%E5%BC%95%E6%93%8E)

## 环境配置
~~~ sh
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=/path/to/depot_tools:$PATH
fetch v8
cd v8
git checkout cbc1b57b90ba1db080f0a045adcefce04c1f21e5
gclient sync
./build/install-build-deps.sh
git remote add wzq https://github.com/Wang-Zhongqi/v8
git merge wzq/main
std_lib = "/path/to/libstdc++.a" # in v8/src/llvm
./tools/dev/gm.py x64.debug
# ./tools/dev/gm.py x64.debug.check
~~~

## 使用LLVM
### LLVM_WASM
1. 额外准备一个js文件，通过js文件调用wasm
    ~~~js
    const bytes = readbuffer(arguments[0]);
    const mod = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(mod, { /* imports */});
    const { add } = instance.exports;

    console.log(add(1, 2));
    ~~~

2. `d8 --llvm-wasm --trace-wasm-compiler /path/to/run_wasm.js -- /path/to/test.wasm`

### LLVM_JS
1. 修改js，添加v8指令
    ~~~js
    function func(a, b) { return a + b; }
    
    console.log(func("a", "b"));
    
    %PrepareFunctionForOptimization(func);  // v8 commands
    %OptimizeFunctionOnNextCall(func);      // v8 commands
    
    console.log(func("a", "b"));
    ~~~

2. `d8 --optimize-on-next-call-optimizes-to-llvm --allow-natives-syntax --trace-opt test.js`

## 开发指南
### 新增LLVM模块
**构建工具**：GN，类似cmake/Cargo

**构建文件**：`.gn`/`BUILD.gn`/`BuildConfig.gn`/`.gni`，类似`CMakeLists.txt`/`Cargo.toml`

**实现方案**：添加一个v8_llvm模块，类似wasmtime中的crates/llvm，参考v8_compiler(turbofan)

1. 配置gn文件依赖 `v8_base_without_compiler` -> `v8_llvm` -> `v8_base`
2. 将本地llvm install目录软链接到third_party
    ~~~sh
    # 建立对本地安装llvm的超链接
    ln -s /home/wangzhongqi/workspace/llvm_16/install/ ./third_party/./third_party/llvm_install 
    ~~~
3. 在gn中，导入LLVM c-api头文件目录
4. 在gn中，调用python脚本，获取与链接LLVM静态库（LLVM对标准库的静态库调用需要额外手动指定）

### LLVM接入V8（js）
**实现方案**：使用llvm替换v8_base模块关于v8_compiler(turbofan)调用，主要是 bytecode -> machine code 流程
1. 定位入口：通过调试stopAtEntry 定位 `v8/src/d8.cc:main`
2. 保证turbofan被调用：通过在d8新增`--allow-natives-syntax`选项，并在js中加入调用优化声明，保证执行turbofan优化
3. 定位`turbofan`调用点：通过增加`--trace-opt`选项并调试， 快速定位turbofan调用点 `v8/src/codegen/compiler.cc:GetOrCompileOptimized`
4. 新增`CodeKind::LLVM`：类似wasmtime中的`Strategy::LLVM`，解决新增枚举导致`switch`缺少对应分支处理导致的编译问题
5. 新增`CompileLLVM`：定位到当前v8有两个优化编译器`CompileTurbofan`/`CompileMaglev`，实现`CompileLLVM`接口
6. 命令行新增编译选项：`--llvm`,`--optimize-on-next-call-optimizes-to-llvm`到`v8/src/flags/flag-definitions.h`
7. 打通编译流程：搜索新增编译选项，补齐调用逻辑
8. 验证`CompileLLVM`函数调用

### LLVM接入V8（wasm）
**文件功能**
1. wasm/module-compiler: wasm编译器最上层接口，提供同步/异步方式，调用底层接口进行编译
2. wasm/module-decoder: 输入wasm字节码，输出解析完成的wasm module
3. wasm/function-compiler: 输入解析完成的wasm module, 输出机器码。类似wasmtime/crates/llvm/src/compiler
4. wasm/function-body-decoder: 实现function to IR(wasmparser + validate + translator)
5. wasm/constant-expression-interface: 实现const/bin/global/ref相关op to IR
6. wasm/wasm-tier: 编译器后端类型，需要新增LLVM
6. wasm/c-api: 通过c-api接口，以api形式调用wasm

**关键类型**
1. `ExecutionTier`：决定后端编译器类型(0:kNone, 1:kLiftoff, 2:kTurbofan)，其中Liftoff表示低优化等级,Turbofan表示高优化等级
2. `WasmFullDecoder`：将wasm translate的公共逻辑提取出来，通过回调函数来完成各个后端的`translate`
3. `Interface`：作为`WasmFullDecoder`的第二个模板参数，需要提供一系列类与回调函数，如op翻译与控制流处理等
4. `AssemblerBuffer`：存放最终返回给v8的类似elf文件的输出
5. `CodeDesc`：记录`AssemblerBuffer`中`instruction/metadata/relocation`的`offset`和`size`
6. `WasmCompilationResult`：将所有编译信息打包成 `WasmCompilationResult`，并返回


**实现方案**
1. 在`wasm/wasm-tier.h:ExecutionTier`中，新增kLLVM选项作为v8 wasm后端选项
2. 在`wasm/function-compiler.cc:ExecuteFunctionCompilation`中，调用`ExecuteLLVMWasmCompilation`
3. 在`wasm/module-compiler.cc:GetDefaultTiersPerModule`中，读取llvm_wasm选项，返回kLLVM
4. 在`llvm/llvm_wasm.cc:ExecuteLLVMWasmCompilation`中，调用`llvm/wasm/compiler`，完成编译流程
4. 在`llvm/wasm/compiler`中，调用`FullDecoder`, wasm bytecode -> LLVM IR
5. 在`llvm/wasm/translate:Translator`类中，实现FullDecoder类的模板成员Interface相关回调函数
6. 在`llvm/wasm/compiler`中，调用接口进行optimise + emit
7. 在`llvm/wasm/compiler`中，对LLVM emit生成的obj按照重新排布，并生成CodeDesc + AssemblerBuffer
8. 在`llvm/wasm/compiler`中，根据输出包装好`WasmCompilationResult`

**问题记录**

* 问题1：在`llvm/llvm-wasm.h`中，由于trampoline(--wasm-generic-wrapper) abi规范与LLVM存在差异，导致无法正常传参

  ~~~sh
  # 参数寄存器
  # turbofan: rdi, rsi, rax, rdx, rcx, rbx, r9
  # LLVM:     rdi, rsi,      rdx, rcx, r8,  r9

  # 返回值寄存器：
  # 均为rax
  ~~~

  解决方案：
  1. 在`wasm/function-compiler.h:CanUseGenericJsToWasmWrapper`中，识别`llvm`并禁用`GenericJsToWasmWrapper`
  2. 在`wasm/function-compiler.cc:JSToWasmWrapperCompilationUnit`中，调用`llvm/llvm_wasm.cc:NewJSToWasmCompilationJob`
  3. 在`llvm/llvm_wasm.cc`中，实现`NewJSToWasmCompilationJob`

* 问题2：在`flags/flag-definitions.h`末尾加上`DEFINE_BOOL`选项，无法修改对应值
  1. 在文件末尾`#undef FLAG`，同时`#define FLAG FLAG_READONLY`，导致在这个宏定义后所有的选项均为只读的

### js编译
### JS字节码处理
**接口调用**
~~~shell
# -e 指定字符串作为js执行，函数调用需要在js中指定
./out/x64.debug/d8 -e "function func(a, b) { return a + b; } console.log(func(1, 1))"

# 在js中调用func，才会被编译（包括to bytecode）
echo 'function func(a, b) { return a + b; } func(1, 1)' > ~/workspace/test.js

# js --Parse-> ast --Interpreter(Ignition)-> bytecode，打印bytecode
./out/x64.debug/d8 --print-bytecode ~/workspace/test.js
~~~

**Implicit Register Base**: `ImplicitRegisterUse` -> `Param` + `Local`

**Bytecode Declare Path**: `v8/src/interpreter/bytecode.h`

**Bytecode Interpretation Path**: `v8/src/interpreter/interpreter-generator.cc`

**字节码分析**: `func(a, b) { return a + b; }`
~~~
0 : 0b 04             Ldar a1
2 : 3b 03 00          Add a0, [0]
5 : af                Return
~~~

**TODO**

**字节码分析**: GlobalEntry
~~~
 0 : 13 00             LdaConstant [0]
 2 : c9                Star1
 3 : 19 fe f7          Mov <closure>, r2
 6 : 68 60 01 f8 02    CallRuntime [DeclareGlobals], r1-r2
11 : 21 01 00          LdaGlobal [1], [0]
14 : c9                Star1
15 : 0d 01             LdaSmi [1]
17 : c8                Star2
18 : 0d 01             LdaSmi [1]
20 : c7                Star3
21 : 66 f8 f7 f6 02    CallUndefinedReceiver2 r1, r2, r3, [2]
26 : ca                Star0
27 : af                Return
~~~

**TODO**

**LdaConstant [0]**: `V(LdaConstant, ImplicitRegisterUse::kWriteAccumulator, OperandType::kIdx)`
~~~arm
ldr acc, ConstPool[0]
~~~

**常量池分析**
~~~
Constant pool (size = 2)
0x307900040011: [TrustedFixedArray]
 - map: 0x0aad00000595 <Map(TRUSTED_FIXED_ARRAY_TYPE)>
 - length: 2
           0: 0x0aad002984d1 <FixedArray[2]>
           1: 0x0aad00298461 <String[4]: #func>
~~~

**TODO**
