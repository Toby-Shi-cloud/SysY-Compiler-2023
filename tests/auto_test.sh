#!/usr/bin/env bash

echo "Running auto_test.sh"

if [[ $# == 1 ]]; then
    binary=$1
    rm -f Compiler
    ln -s "$binary" Compiler
fi

clang -emit-llvm -c libsysy/libsysy.c -S -o libsysy/libsysy.ll

for suit in C B A; do
    echo "=====$suit====="
    for ((i=1;i<=30;i++)); do
        ./link.sh $suit $i
        ./run.sh && diff output.txt testfile.out
        if [[ $? != 0 ]]; then
            echo "Test $suit $i failed!"
            exit 1
        fi
        echo "Test $i passed!"
    done
done
