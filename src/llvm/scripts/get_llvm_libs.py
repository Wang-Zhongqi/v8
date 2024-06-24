#!/usr/bin/env python3

import subprocess;
import sys;

llvm_libs = subprocess.run([sys.argv[1], "--libs"], stdout=subprocess.PIPE).stdout
llvm_libs = str(llvm_libs, "utf-8").replace("\n", "").split(" ")
linkage = subprocess.run([sys.argv[1], "--shared-mode"], stdout=subprocess.PIPE).stdout
if linkage == b"static\n": 
    for index, llvm_lib in enumerate(llvm_libs):
        print(llvm_lib.replace("-l", "lib") + ".a")
