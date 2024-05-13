#!/usr/bin/env python3

import os, sys, getopt
from threading import Thread

run_sh = ''
correct_list = []
wrong_list = []


def usage():
    print('Usage: ./auto_test.py <llvm/mips> [-s path_to_compiler] {-d test_suit}')
    exit(-1)


def run_test(test_file, input_file, output_file):
    obj_file = test_file + '.o'
    ans_file = test_file + '.ans'
    ret = os.system(f'{run_sh} {test_file} {obj_file} {input_file} {ans_file} && diff {output_file} {ans_file}')
    if ret == 0:
        correct_list.append(test_file)
    else:
        wrong_list.append(test_file)


def run_suit(test_dir):
    thd = []
    for file in os.listdir(test_dir):
        if file.endswith('.sy') and not file.startswith('.'):
            test_file = os.path.join(test_dir, file)
            input_file = os.path.join(test_dir, file[:-3] + '.in')
            if not os.path.exists(input_file):
                input_file = "/dev/null"
            output_file = os.path.join(test_dir, file[:-3] + '.out')
            t = Thread(target=run_test, args=(test_file, input_file, output_file))
            thd.append(t)
            t.start()
    for t in thd:
        t.join()
    with open(f"{test_dir}.log", 'w') as f:
        print(f"| {test_dir} | {len(correct_list)} | {len(wrong_list)} |", file=f)
    if len(wrong_list) != 0:
        exit(1)


if len(sys.argv) < 2:
    usage()

if __name__ == '__main__':
    try:
        opts = getopt.getopt(sys.argv[2:], "s:d:")
    except getopt.GetoptError as e:
        print("Error:", e)
        usage()

    if sys.argv[1] not in ["llvm", "mips"]:
        print("Error: invalid test type:", sys.argv[1])
        usage()

    run_sh = "./run_{}.sh".format(sys.argv[1])
    path_to_compiler = ''
    test_suit = []
    for opt, arg in opts[0]:
        if opt == '-s':
            path_to_compiler = arg
        elif opt == '-d':
            test_suit.append(arg)

    if path_to_compiler != '':
        os.system('rm -f ./Compiler')
        os.system('ln -s {} ./Compiler'.format(path_to_compiler))
    if not test_suit:
        usage()

    os.system('clang -emit-llvm -c libsysy/libsysy.c -S -o libsysy/libsysy.ll')
    for suit in test_suit:
        run_suit(suit)
