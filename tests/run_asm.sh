#!/usr/bin/env bash

file="`basename $2 .S`"
temp="temp-$file"
path="`realpath $3`"
mount="`pwd`/$temp:/root/sysy"

f2="/root/sysy/`basename $2`"
f3="/root/sysy/`basename $3`"
f4="/root/sysy/`basename $4`"
f5="/root/sysy/`basename $5`"

function my-exit() { rm -rf $temp; exit 1; }

# run Compiler
./Compiler "$1" -S -o "$2" ${@:6} || exit 1
mkdir $temp || exit -1
cp "$2" $temp
# link with libsysy
docker run --rm -v $mount -e ARCH=riscv ghcr.io/tobisc-v/sysy:riscv /usr/bin/sysy-elf.sh $f2 || my-exit
# run elf
docker run --rm -v $mount -e ARCH=riscv -a STDIN -a STDOUT -a STDERR ghcr.io/tobisc-v/sysy:riscv /usr/bin/sysy-run-elf.sh /root/sysy/$file.elf <$3 >$4 2>$5
code=$?
if [ ! -z "$(cat "$4")" ] && [ $(tail -n1 "$4" | wc -l) -eq 0 ]; then
    echo '' >> "$4"
fi
echo $code >> "$4"
rm -rf $temp
