#!/usr/bin/env bash

./Compiler testfile.txt -emit-llvm testfile.ll \
&& llvm-link -o testfile.sylib.bc testfile.ll libsysy/libsysy.ll \
&& lli testfile.sylib.bc < input.txt > testfile.out \
&& rm -f testfile.sylib.bc
