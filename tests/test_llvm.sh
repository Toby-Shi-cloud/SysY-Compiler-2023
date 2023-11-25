#!/usr/bin/env bash

llvm-link -o testfile.sylib.bc testfile.ll libsysy/libsysy.ll \
&& lli testfile.sylib.bc < input.txt > testfile.out \
&& rm -f testfile.sylib.bc \
&& diff testfile.out output.txt
