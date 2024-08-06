#!/usr/bin/env python3

import os, sys, getopt
import subprocess
import multiprocessing


obj_suffix = ''


def usage():
    print('Usage: ./auto_test.py <llvm/asm> [-O opt_level] [-s path_to_compiler] {-d test_suit}')
    exit(-1)


def test_runner(args):
    return run_test(*args[0], *args[1])


def run_test(run_sh, opt_level, test_file, input_file, output_file):
    obj_file = test_file + obj_suffix
    ans_file = test_file + '.ans'
    err_file = test_file + '.err'
    try:
        p = subprocess.run([run_sh, test_file, obj_file, input_file, ans_file, err_file, f'-O{opt_level}'],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120, check=True)
        t = p.stderr.decode()
    except subprocess.CalledProcessError as err:
        return False, err.stdout.decode() + '\n' + err.stderr.decode()
    except subprocess.TimeoutExpired:
        if obj_suffix == ".S": os.system(f"docker kill sysy-`basename {test_file}`")
        return False, 'Time Limit Exceeded'
    try:
        subprocess.run(['diff', output_file, ans_file], capture_output=True, check=True)
    except subprocess.CalledProcessError:
        return False, 'Wrong Answer'
    return True, t


def run_suit(test_dir, run_sh, opt_level):
    tasks = []
    for file in os.listdir(test_dir):
        if file.endswith('.sy') and not file.startswith('.'):
            test_file = os.path.join(test_dir, file)
            input_file = os.path.join(test_dir, file[:-3] + '.in')
            if not os.path.exists(input_file):
                input_file = "/dev/null"
            output_file = os.path.join(test_dir, file[:-3] + '.out')
            tasks.append((test_file, input_file, output_file, ))
    pool = multiprocessing.Pool()
    results = pool.map(test_runner, map(lambda t: ((run_sh, opt_level), t), tasks))
    pool.close()
    pool.join()
    os.system("rm -r temp-*")
    passed = sum(map(lambda res: 1 if res[0] else 0, results))
    with open(f"{test_dir}.log", 'w') as f:
        print(f"| {test_dir} | {passed} | {len(results) - passed} |", file=f)
    if len(results) - passed == 0:
        return
    results = map(lambda x: (os.path.basename(x[1][0])[:2], x[0]), zip(results, tasks))
    results = sorted(results, key=lambda x: x[0])
    for idx, res in results:
        if res[0]: continue
        print('**********')
        msg = [f"Failed on {idx}"] + res[1].split('\n')
        print('\n\t'.join(msg))
        print()
    print('**********')
    print(f"Passed: {passed} / {len(results)}")
    exit(1)


def main():
    if len(sys.argv) < 2:
        usage()
    try:
        opts = getopt.getopt(sys.argv[2:], "s:d:O:")
    except getopt.GetoptError as e:
        print("Error:", e)
        usage()

    if sys.argv[1] not in ["llvm", "asm"]:
        print("Error: invalid test type:", sys.argv[1])
        usage()
    global obj_suffix
    obj_suffix = ('.ll' if sys.argv[1] == 'llvm' else '.S')

    run_sh = f"./run_{sys.argv[1]}.sh"
    path_to_compiler = ''
    test_suit = []
    opt_level = 1
    for opt, arg in opts[0]:
        if opt == '-s':
            path_to_compiler = arg
        elif opt == '-d':
            test_suit.append(arg)
        elif opt == '-O':
            opt_level = int(arg)

    if path_to_compiler != '':
        os.system('rm -f ./Compiler')
        os.system('ln -s {} ./Compiler'.format(path_to_compiler))
    if not test_suit:
        usage()

    os.system('clang -emit-llvm -c libsysy/libsysy.c -S -o libsysy/libsysy.ll')
    for suit in test_suit:
        run_suit(suit, run_sh, opt_level)


if __name__ == '__main__':
    main()
