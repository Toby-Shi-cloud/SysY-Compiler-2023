# SysY Compiler 2023 (C++)

项目地址：https://github.com/Toby-Shi-cloud/SysY-Compiler-2023

BUAA Compiler Course Project 2023 by Toby Shi.

前排提示：
为了弥补2023年编译大赛摆烂的遗憾（[Tobisc](https://github.com/Tobisc-V)），
我决定陆续在编译课程要求的基础上进行开发，完成大赛的全部语法，并视情况考虑开发ARM32-V7后端和RISCV64后端。
对于希望参考本人实验课的代码的同学请checkout到tag [experiment](https://github.com/Toby-Shi-cloud/SysY-Compiler-2023/releases/tag/experiment)。
本人实验课最后一次提交采用的代码是：[01748417](https://github.com/Toby-Shi-cloud/SysY-Compiler-2023/tree/01748417d1447f8c52164ffef58d5e196b40aa5c)

## Build

- c++ compiler = clang 10.0.0
- c++ standard = c++17

### Dependencies
- ["dbg.h"](https://github.com/sharkdp/dbg-macro) for debug (no need for release mode)
- ["magic_enum"](https://github.com/Neargye/magic_enum) for enum reflection

You can download this library manually or use `download.py` (`gh` needed) to install them.

## Usage

直接不带参数运行的版本是符合实验要求的版本，如果需要调整请参考

https://github.com/Toby-Shi-cloud/SysY-Compiler-2023/blob/71adc9706254cb6141f7575b7c803e30c03bb95f/src/main.cpp#L14-L31
