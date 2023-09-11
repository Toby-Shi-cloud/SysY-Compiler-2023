#!/bin/zsh
# zip.sh

rm -f source.zip

zip source.zip src/
tree -fi --noreport src | while IFS='' read -r line
do
  zip -u source.zip "$line"
done
