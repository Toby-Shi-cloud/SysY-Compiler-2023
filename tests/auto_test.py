#!/usr/bin/env python3

import os, sys, getopt

run_sh = ''


def usage():
    print('Usage: ./auto_test.py <llvm/mips> [-s path_to_compiler] {-d test_suit}')
    exit(-1)


def run_test(test_file, input_file, output_file):
    os.system('cp {} ./testfile.txt'.format(test_file))
    os.system('cp {} ./input.txt'.format(input_file))
    os.system('cp {} ./output.txt'.format(output_file))
    ret = os.system(run_sh + ' && diff output.txt testfile.out')
    if ret != 0:
        print('Test {} failed'.format(test_file), flush=True)
        exit(1)
    print('Test {} passed'.format(test_file), flush=True)


def run_test_e(test_file, error_file):
    os.system('cp {} ./testfile.txt'.format(test_file))
    os.system('cp {} ./stderr.txt'.format(error_file))
    ret = os.system('./Compiler && diff stderr.txt error.txt')
    if ret != 0:
        print('Test {} failed'.format(test_file), flush=True)
        exit(1)
    print('Test {} passed'.format(test_file), flush=True)


def run_suit(test_dir):
    print('====={}====='.format(test_dir), flush=True)
    for test_id in range(1, 31):
        test_file = os.path.join(test_dir, 'testfile' + str(test_id) + '.txt')
        input_file = os.path.join(test_dir, 'input' + str(test_id) + '.txt')
        output_file = os.path.join(test_dir, 'output' + str(test_id) + '.txt')
        error_file = os.path.join(test_dir, 'error' + str(test_id) + '.txt')
        if not os.path.isfile(test_file):
            continue
        if os.path.isfile(error_file):
            run_test_e(test_file, error_file)
        else:
            run_test(test_file, input_file, output_file)


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
        test_suit = ['A1', 'B1', 'C1', 'D1', 'A2', 'B2', 'C2', 'D2']

    os.system('clang -emit-llvm -c libsysy/libsysy.c -S -o libsysy/libsysy.ll')
    for suit in test_suit:
        run_suit(suit)
