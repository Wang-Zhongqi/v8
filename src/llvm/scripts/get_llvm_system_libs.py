#!/usr/bin/env python3

import subprocess;
import sys;

llvm_system_libs = subprocess.run([sys.argv[1], "--system-libs"], stdout=subprocess.PIPE).stdout
llvm_system_libs = str(llvm_system_libs, "utf-8").replace("\n", "").split(" ")
linkage = subprocess.run([sys.argv[1], "--shared-mode"], stdout=subprocess.PIPE).stdout
if linkage == b"static\n": 
    for index, llvm_system_lib in enumerate(llvm_system_libs):
        print(llvm_system_lib.replace("-l", "/usr/lib/" + sys.argv[2] + "-linux-gnu/lib") + ".so")
    print("/usr/lib/gcc/" + sys.argv[2] + "-linux-gnu/9/libstdc++.a")