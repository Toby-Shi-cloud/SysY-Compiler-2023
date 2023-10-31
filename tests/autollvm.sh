#!/bin/zsh

if [[ $1 == clean ]]
then
    rm -f testfile.c testfile.s testfile.ll
    exit 0
fi

printf 'int getint() { int x; scanf("%%d", &x); return x; } \n' > testfile.c
cat testfile.txt >> testfile.c

clang --target=mips -Wno-implicit-function-declaration "$@" -std=c89 -S -emit-llvm testfile.c
llc -march=mips -mcpu=mips32 testfile.ll -o testfile.s

rm -f testfile.c

