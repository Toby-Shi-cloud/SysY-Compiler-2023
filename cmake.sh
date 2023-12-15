#!/usr/bin/env bash

cmake -B build/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/home/codespace/llvm/bin/clang -DCMAKE_CXX_COMPILER=/home/codespace/llvm/bin/clang++ \
&& cmake --build build/ --config Release \
&& cd build/ && ctest -V -C Release -R 'MIPS-*'
