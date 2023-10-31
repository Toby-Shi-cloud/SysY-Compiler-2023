#!/bin/zsh
# zip.sh

rm -f source.zip
zip -r -X source.zip CMakeLists.txt src/
