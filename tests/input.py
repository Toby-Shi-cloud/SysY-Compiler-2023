#! /usr/bin/python3
# -*- coding: utf-8 -*-

lines = open("input.txt", encoding="utf-8").readlines()
for line in lines:
    L = map(int, line.strip().split())
    for x in L:
        print(x)
