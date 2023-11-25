#!/usr/bin/env bash

./input.py | java -jar mars.jar nc testfile.s > testfile.out \
&& diff testfile.out output.txt
