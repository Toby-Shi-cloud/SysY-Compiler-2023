#!/usr/bin/env bash

./Compiler testfile.txt -S testfile.s \
&& ./input.py < input.txt | java -jar mars.jar nc testfile.s > testfile.out
