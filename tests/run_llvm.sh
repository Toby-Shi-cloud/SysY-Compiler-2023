#!/usr/bin/env bash

./Compiler "$1" -no-S -emit-llvm "$2" ${@:6} \
&& llvm-link -o "$2".bc "$2" libsysy/libsysy.ll
if [[ $? -ne 0 ]]; then
    exit 1
fi

lli "$2".bc < "$3" > "$4" 2> "$5"
code=$?
if [ ! -z "$(cat "$4")" ] && [ $(tail -n1 "$4" | wc -l) -eq 0 ]; then
    echo '' >> "$4"
fi
echo $code >> "$4"

rm -f "$2".bc
