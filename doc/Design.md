# SysY 2023 编译器设计文档

## 参考编译器介绍

本编译器在设计时，主要参考了 [LLVM-Clang](https://clang.llvm.org/) 的设计。
LLVM-Clang 是一个 C 系列编译器，其优秀的设计和实现都是业界公认的。
当然 Clang 的完善程度自然不是我这个小编译器可以比拟的，但是我还是希望能够借鉴一些 Clang 的设计思想。

### 总体结构

[LLVM](https://github.com/llvm/llvm-project) 编译器是一个庞大的项目，总体上来说，它由以下几个主要子项目组成：
1. clang - clang 是 LLVM 的前端，负责将 C 系列语言翻译成 LLVM IR。
2. opt - opt 是 LLVM 的优化器，负责对 LLVM IR 进行各种优化。
3. llc - llc 是 LLVM 的后端，负责将 LLVM IR 翻译成目标机器的汇编代码。
4. lld - lld 是 LLVM 的链接器，负责链接多个目标代码生成可执行文件。
5. lldb - lldb 是 LLVM 的调试器，负责调试可执行文件。
6. libc - libc 是 LLVM 的 C 标准库实现，负责提供 C 标准库的各种函数实现。
7. ...

### 接口设计

LLVM 的各个子项目之间的接口设计非常清晰，每个子项目都有自己的接口，而且接口之间的依赖关系也非常清晰。
每个子项目既可以单独调用，也可以整体使用。
LLVM 的设计将高内聚、低耦合的思想发挥到了极致。

### 文件组织

LLVM 代码在 [GitHub](https://github.com/llvm/llvm-project) 上开源，其代码文件组织简单但高效又清晰，是非常优雅的设计。

LLVM 项目根目录下是其各个子项目目录，每个子项目大多都由 `cmake`, `docs`, `examples`, `include`, `lib`, `test`, `unittests` 等目录组成。

## 编译器总体设计

### 总体结构

参考 LLVM 的设计，本编译器也计划分为以下几个子项目：
1. 前端 - 负责将 SysY 语言 (C 语言子集) 翻译成 IR。为了方便测试，我计划直接采用 LLVM IR 作为中间表示。
2. 优化器 - 负责对 IR 进行各种优化。
3. 后端 - 负责将 IR 翻译成目标机器的汇编代码 (mips)。

### 接口设计

参考 LLVM 的设计，本编译器也计划分离各个子项目，使他们既可以单独使用也可以整体调用。

### 文件组织

参考 LLVM 的设计，本编译器计划的文件结构如下：
```
.
├── cmake-build-* (build directory)
│   └── ...
├── lib (library directory)
│   └── ...
├── doc (document directory)
│   ├── Design.md
│   └── SysY2023.g4
├── src (source code directory)
│   ├── frontend
│   │   ├── lexer*
│   │   ├── parser*
│   │   ├── visitor*
│   │   └── main.cpp
│   ├── mir
│   │   └── ...
│   ├── opt
│   │   └── ...
│   ├── backend
│   │   └── ...
│   ├── dbg.h
│   └── main.cpp
├── testfiles (test files directory)
│   └── ...
├── CMakeLists.txt
├── LICENSE
├── README.md
└── zip.sh
```

## 词法分析设计

namespace `frontend::lexer`

### Token 设计

Token 包含如下信息：
```cpp
struct Token {
    token_type_t type;
    std::string_view raw;
    size_t line;
    size_t column;
};
```

### Lexer 设计

Lexer 类主要包含以下接口：
```cpp
class Lexer {
public:
    size_t line() const; // 当前解析到的行号
    size_t column() const; // 当前解析到的列号
    Iterator begin(); // 返回指向下一个 Token 的迭代器
    Iterator end(); // 返回空迭代器，作为 Token 流的结束标志
    std::optional<Token> next_token(); // 返回下一个 Token
};
```

Lexer 类主要实现方案如下：
```cpp
class Lexer {
private:
    std::optional<Token> next_token_impl(); // 返回下一个 Token
    void next_token_skip_whitespaces(); // 跳过空白字符
    bool next_token_skip_comment(); // 跳过注释
    std::optional<Token> next_token_try_word(); // 尝试解析关键字和运算符
    std::optional<Token> next_token_try_number(); // 尝试解析数字
    std::optional<Token> next_token_try_string(); // 尝试解析字符串
    std::optional<Token> next_token_try_identifier(); // 尝试解析标识符
    Token next_token_error_token(); // 解析错误时返回的 Token
};
```

## 语法分析设计

namespace `frontend::parser`

总体设计思想：使用声明式语法实现递归下降语法分析器。

- 优势：简单直观、易于理解、易于扩展、代码复用率高，优雅永不过时。
- 劣势：性能有损失、不支持左递归和大范围回溯、调试难度略微增加。

### 语法节点

namespace `frontend::grammar`

仅分为了常规语法节点和终结符语法节点两种，定义如下：
```cpp
struct GrammarNode {
    const grammar_type_t type;
    std::vector<pGrammarNode> children;
};

struct TerminalNode : GrammarNode {
    const lexer::Token token;
};

using pGrammarNode = std::unique_ptr<GrammarNode>;
using pTerminalNode = std::unique_ptr<TerminalNode>;
```

### AST 生成器

AST 生成器，其本质是一个函数，接受一个 Parser 作为参数，返回语法节点链表。定义如下：
```cpp
using generator_t = std::function<optGrammarNodeList(SysYParser *)>;
```

支持下面 4 个操作符重载：
```cpp
generator_t operator+(const generator_t &one, const generator_t &other); // 串联
generator_t operator|(const generator_t &one, const generator_t &other); // 选择
generator_t operator*(const generator_t &gen, _option); // 可选
generator_t operator*(const generator_t &gen, _many); // 若干
```

### SysYParser

SysYParser 类主要有以下方法：
```cpp
class SysYParser {
    template<lexer::token_type_t type>
    inline static auto generator() -> generator_t; // 生成一个匹配指定 Token 类型的 generator
    
    template<grammar_type_t type>
    inline static auto generator() -> generator_t; // 生成一个匹配指定语法类型的 generator
    
    inline pGrammarNode grammarNode(grammar_type_t type, const generator_t &gen); // 生成一个语法节点
    
    template<grammar_type_t>
    pGrammarNode parse_impl(); // 解析指定语法类型的语法节点
    
public:
    void parse(); // 解析整个程序
};
```
