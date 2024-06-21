#!/usr/bin/env bash

echo '#include"libsysy/libsysy.h"' > testfile.c
cat testfile.sy >> testfile.c

clang testfile.c -std=c99 -emit-llvm -S -o testfile.std.ll "$@"
clang testfile.c --target=arm64 -std=c99 -S -o testfile.arm.asm "$@"
clang testfile.c --target=riscv64 -std=c99 -S -o testfile.riscv.asm "$@"

rm -f testfile.c
