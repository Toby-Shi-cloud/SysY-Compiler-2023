#!/usr/bin/env bash

echo '#include"libsysy/libsysy.h"' > testfile.c
cat testfile.sy >> testfile.c

clang testfile.c -std=c99 -emit-llvm -S -o testfile.std.ll "$@"
llc testfile.std.ll -o testfile.std.asm

rm -f testfile.c

