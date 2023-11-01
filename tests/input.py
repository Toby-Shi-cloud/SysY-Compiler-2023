#! /usr/bin/python3

try:
    while True:
        L = map(int, input().split())
        for x in L:
            print(x)
except EOFError:
    pass
