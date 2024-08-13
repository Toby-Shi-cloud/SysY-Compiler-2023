#!/usr/bin/env python3

import os, sys, getopt, csv, re
import subprocess
import multiprocessing


def usage():
    print('Usage: ./auto_test.py <llvm/asm> [-O opt_level] [-s path_to_compiler] {-d test_suit}')
    exit(-1)


def write_csv(path: str, title: list, data: list):
    with open(path, "w") as f:
        writer = csv.writer(f)
        writer.writerow(title)
        writer.writerows(data)


def write_table(path: str, title: list, data: list):
    with open(path, "w") as writer:
        print("| " + " | ".join(title) + " |", file=writer)
        print("|" + " --- |" * len(title), file=writer)
        for line in data:
            print("| " + " | ".join(line) + " |", file=writer)


def read_csv(path: str):
    with open(path, "r") as f:
        reader = csv.reader(f)
        title = next(reader)
        return title, list(reader)


def strptime(time_str: str):
    pos = 7 if time_str.startswith("TOTAL: ") else 0
    pattern = re.compile(r"(\d+)H-(\d+)M-(\d+)S-(\d+)us")
    match = pattern.match(time_str, pos)
    if match is None: return None
    caps = match.groups()
    return (int(caps[0]) * 3600 + int(caps[1]) * 60 + int(caps[2])) * 1000000 + int(caps[3])


def strftime(time: int):
    sec = time // 1000000
    return f"{sec // 3600}H-{sec // 60 % 60}M-{sec % 60}S-{time % 1000000}us"


class TestRunner:
    def __init__(self, run_sh: str, opt_level: str, obj_suffix: str, test_dir: str, perf: bool):
        self.run_sh = run_sh
        self.opt_level = opt_level
        self.obj_suffix = obj_suffix
        self.perf = perf
        self.test_dir = test_dir

    def run_test_wrap(self, args):
        return self.run_test(*args)

    def run_test(self, name: str, in_file: str, std_file: str):
        '''
        $name.sy -> $name.ll/$name.s
        $name.in -> $name.myout + $name.myerr
        answer: $name.out
        '''
        src_file = name + '.sy'
        obj_file = name + obj_suffix
        out_file = name + '.myout'
        err_file = name + '.myerr'
        try:
            timeout = 300 if self.perf else 120
            subprocess.run(
                [self.run_sh, src_file, obj_file, in_file, out_file, err_file, self.opt_level],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, check=True)
        except subprocess.CalledProcessError as err:
            return False, err.stdout.decode() + '\n' + err.stderr.decode()
        except subprocess.TimeoutExpired:
            if obj_suffix == ".S": os.system(f"docker kill sysy-`basename {name}`")
            return False, 'Time Limit Exceeded'
        try:
            subprocess.run(['diff', '-Z', std_file, out_file], capture_output=True, check=True)
        except subprocess.CalledProcessError:
            return False, 'Wrong Answer '
        msg = subprocess.check_output(['tail', '-1', err_file]).decode() if self.perf else ''
        return True, msg

    def run_suit(self):
        tasks = []
        for file in os.listdir(self.test_dir):
            if file.endswith('.sy') and not file.startswith('.'):
                test_file = os.path.join(self.test_dir, file)[:-3]
                input_file = os.path.join(self.test_dir, file[:-3] + '.in')
                if not os.path.exists(input_file):
                    input_file = "/dev/null"
                output_file = os.path.join(self.test_dir, file[:-3] + '.out')
                tasks.append((test_file, input_file, output_file, ))
        pool = multiprocessing.Pool()
        results = pool.map(self.run_test_wrap, tasks)
        pool.close()
        pool.join()
        passed = sum(map(lambda res: 1 if res[0] else 0, results))
        with open(f"{self.test_dir}.log", 'w') as f:
            print(f"| {self.test_dir} | {passed} | {len(results) - passed} |", file=f)
        if self.perf:
            if not os.path.exists("perf"): os.mkdir("perf")
            my_hash = subprocess.check_output(["/bin/sh", "-c", "git log --oneline -1 | awk '{print $1}'"]).decode().strip()
            csv_file = f"perf/{self.test_dir}.csv"
            table_file = f"perf/{self.test_dir}.md"
            performance = [(os.path.basename(args[0]),
                            'ACCEPTED' if res[0] else res[1].strip().replace('\n', '<br>'),
                            res[1].strip().replace('\n', '<br>') if res[0] else '')
                    for args, res in zip(tasks, results)]
            performance.sort(key=lambda x: x[0])
            this_time = [strptime(line[2]) for line in performance]
            try:
                last_time_raw = read_csv(csv_file)
                last_time_dict = {x[0]: strptime(x[1]) for x in last_time_raw[1]}
                last_time_hash = {x[0]: x[2] for x in last_time_raw[1]} if len(last_time_raw[0]) == 3 else {}
                last_time = [(last_time_dict.get(x[0]), last_time_hash.get(x[0])) for x in performance]
            except FileNotFoundError:
                last_time = [(None, None) for _ in range(len(performance))]
            increase = [(y - x) / x if x is not None and y is not None else None
                        for x, (y, _) in zip(this_time, last_time)]
            data = []
            bests = []
            for (name, status, msg), time_, (last_, that_hash), inc in zip(performance, this_time, last_time, increase):
                if last_ is None: best = (time_, my_hash if time_ is not None else None)
                elif time_ is None: best = (last_, that_hash)
                elif last_ > time_: best = (time_, my_hash)
                else: best = (last_, that_hash)
                time = strftime(time_) if time_ is not None else msg
                last = strftime(last_) if last_ is not None else ''
                color = 'red' if inc is not None and inc < 0 else 'green'
                inc = fr"${{\textsf{{\color{{{color}}}{inc*100:+.1f}\%}}}}$" if inc is not None else ''
                data.append((name, status, time, last, that_hash or '', inc, ))
                bests.append((name, strftime(best[0]) if best[0] is not None else '', best[1], ))
            write_table(table_file, ['TestName', 'Status', 'Time', 'LastBest', 'BestHash', 'SpeedUp'], data)
            write_csv(csv_file, ['TestName', 'Time', 'Hash'], bests)
        if len(results) - passed == 0:
            return
        results = map(lambda x: (os.path.basename(x[1][0]), x[0]), zip(results, tasks))
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
        runner = TestRunner(run_sh, f'-O{opt_level}', obj_suffix, suit, 'perf' in suit)
        runner.run_suit()


if __name__ == '__main__':
    main()
