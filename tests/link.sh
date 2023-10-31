#!/usr/bin/env bash

if [[ $# -gt 2 ]]; then
    echo "Usage: $0 <data-name> [number]"
    exit 1
fi

mydir=$(dirname "$0")
rm -f "$mydir"/testfile.txt "$mydir"/input.txt "$mydir"/output.txt

if [[ $# == 0 ]]; then
  touch "$mydir"/testfile.txt "$mydir"/input.txt "$mydir"/output.txt
  exit 0
fi

datadir=$1

if [[ $# == 2 ]]; then
  number=$2
fi

cp "$datadir"/testfile"$number".txt "$mydir"/testfile.txt
cp "$datadir"/input"$number".txt "$mydir"/input.txt
cp "$datadir"/output"$number".txt "$mydir"/output.txt
