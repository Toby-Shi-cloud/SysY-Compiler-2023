#!/usr/bin/env bash
# zip.sh

rm -f source.zip
zip -r -X source.zip CMakeLists.txt src/
