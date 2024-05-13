#!/usr/bin/env bash

./Compiler "$1" -no-S -emit-llvm "$2" $5 \
&& llvm-link -o "$2".bc "$2" libsysy/libsysy.ll
if [[ $? -ne 0 ]]; then
    exit 1
fi

lli "$2".bc < "$3" > "$4"
echo $? >> "$4"

rm -f "$2".bc
