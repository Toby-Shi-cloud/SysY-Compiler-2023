#!/usr/bin/env bash

echo "Running auto_test.sh"

if [[ $# -eq 0 ]]; then
    echo "Usage: ./auto_test.sh <llvm/mips> [path_to_compiler] [test_suit]"
    exit 1
fi

run_sh="./run_$1.sh"

if [[ $# -ge 2 ]]; then
    binary=$2
    rm -f Compiler
    ln -s "$binary" Compiler
fi

clang -emit-llvm -c libsysy/libsysy.c -S -o libsysy/libsysy.ll

function test_suit() {
    dir=$1
    suit=${dir:0:1}
    num=${dir:1}
    echo "=====$suit====="
    for ((i=1;i<=num;i++)); do
        ./link.sh "$suit" "$i"
        $run_sh && diff output.txt testfile.out
        if [[ $? != 0 ]]; then
            echo "Test $suit $i failed!"
            exit 1
        fi
        echo "Test $i passed!"
    done
}

if [[ $# -ge 3 ]]; then
    test_suit "$3"
    exit 0
fi

for dir in A30 B30 C30 D6; do
    test_suit $dir
done
