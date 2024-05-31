#!/usr/bin/env python3

import os
import json
import shutil
import subprocess

GH_PREFIX = 'gh api ' \
            '-H "Accept: application/vnd.github+json" ' \
            '-H "X-GitHub-Api-Version: 2022-11-28" '
TEMP_TAR = os.path.join('include', 'temp.tar.gz')


def download(owner: str, repo: str):
    release = json.load(os.popen(f'{GH_PREFIX} /repos/{owner}/{repo}/releases/latest'))
    tar = subprocess.run(f'{GH_PREFIX} {release["tarball_url"]}', shell=True, capture_output=True).stdout
    open(TEMP_TAR, 'wb').write(tar)
    shutil.unpack_archive(TEMP_TAR, "include")


def download_dbg_macro():
    print('Downloading dbg_macro...')
    download('sharkdp', 'dbg-macro')
    dir_name = list(filter(lambda x: x.startswith('sharkdp-dbg-macro-'), os.listdir('include')))
    if len(dir_name) == 0:
        raise FileNotFoundError('Directory not found: sharkdp-dbg-macro-*')
    shutil.move(os.path.join('include', dir_name[0], 'dbg.h'), 'include/dbg_macro.h')
    shutil.rmtree(os.path.join('include', dir_name[0]))


def download_magic_enum():
    print('Downloading magic_enum...')
    download('Neargye', 'magic_enum')
    dir_name = list(filter(lambda x: x.startswith('Neargye-magic_enum-'), os.listdir('include')))
    if len(dir_name) == 0:
        raise FileNotFoundError('Directory not found: Neargye-magic_enum-*')
    shutil.move(os.path.join('include', dir_name[0], 'include', 'magic_enum', 'magic_enum.hpp'), 'include')
    shutil.rmtree(os.path.join('include', dir_name[0]))


def download_clipp():
    print('Downloading clipp...')
    download('muellan', 'clipp')
    dir_name = list(filter(lambda x: x.startswith('muellan-clipp-'), os.listdir('include')))
    if len(dir_name) == 0:
        raise FileNotFoundError('Directory not found: muellan-clipp-*')
    shutil.move(os.path.join('include', dir_name[0], 'include', 'clipp.h'), 'include')
    shutil.rmtree(os.path.join('include', dir_name[0]))


def main():
    if os.path.exists("include"):
        print("Directory 'include' already exists.")
        print("Do you want to remove it? [y/n]", end=' ')
        if input().lower() == 'y':
            shutil.rmtree('include')
        else:
            print("Please remove 'include' directory and run this script again.")
            return
    os.makedirs('include', exist_ok=True)
    download_dbg_macro()
    download_magic_enum()
    download_clipp()
    if os.path.exists(TEMP_TAR):
        os.remove(TEMP_TAR)


if __name__ == '__main__':
    main()
