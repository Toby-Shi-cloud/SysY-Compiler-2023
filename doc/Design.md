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

参考 LLVM 的设计，本编译器的文件结构如下：
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
│   ├── backend
│   │   ├── translator.h
│   │   ├── reg_alloca.h
│   │   └── ...
│   ├── frontend
│   │   ├── lexer.h
│   │   ├── parser.h
│   │   ├── visitor.h
│   │   └── ...
│   ├── mips
│   │   ├── operand.h
│   │   ├── instruction.h
│   │   ├── component.h
│   │   └── ...
│   ├── mir
│   │   ├── type.h
│   │   ├── value.h
│   │   ├── derived_value.h
│   │   ├── instruction.h
│   │   └── ...
│   ├── opt
│   │   └── ...
│   ├── dbg.h
│   ├── enum.h
│   └── main.cpp
├── tests (test files directory)
│   └── ...
├── CMakeLists.txt
├── LICENSE
├── README.md
└── ...
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

## 错误处理设计 & 中间代码生成设计

- namespace `frontend::visitor`
- namespace `mir`

我的错误处理和中间代码生成均主要在 visitor 中实现。

### visitor 方法设计

visitor 中有如下定义：

```cpp
using value_type = mir::Value *;
using value_list = std::list<value_type>;
using value_vector = std::vector<value_type>;
using return_type = std::pair<value_type, value_list>;

template<grammar_type_t type>
return_type visit(const GrammarNode &node);
```

这个函数模板针对每一个语法节点进行了一种模板特化，当然对于不需要特殊处理的语法节点，也有默认实现（访问所有子节点）。

函数模板的返回值是该语法节点的值和该语法节点生成的全部中间代码，当然如果出现错误，则返回空值，并想 `message_queue` 添加编译提示信息。

### 符号表设计

符号表是一个栈，每个栈内拥有一个 `std::unordered_map` 是从符号名到符号信息的映射。
> 由于 C++ 的栈不支持遍历，所以这里直接使用了 `std::deque` 来实现栈。

这个符号表仅服务于 `visitor`，在 `visitor` 生成中间代码后，符号表信息将转移至 `mir::Value` 之间的相互引用关系中。

### 中间代码设计

中间代码采用 LLVM IR 的设计，当然对原版有所简化和改动。但其中心思想仍然是通过 `Use` 串联 `Value` 和 `User`，用 `BasicBlock` 来组织 `Instruction` 的 SSA 设计。

在中间代码中，我为每个指令都建立了一个类，这样可以方便优化和访问。

为了简化中间代码生成的难度，我在大多数时候使用了 LLVM IR 的 opaque pointer (模糊指针)，避免了一些不必要的 `bitcast` 或 `getelementptr` 这样的指针变换和计算指令。

## 代码生成设计

- namespace `backend`
- namespace `mips`

生成最终的 mips 代码的难度其实并不如想象中的那么简单，主要是 LLVM IR 的设计和 mips 的设计并非总是一一对应的，有些部分必须要进行一些转换。
因此我设计的后端先将 LLVM IR 转换成含有虚拟寄存器的 mips 代码，随后再通过寄存器分配将虚拟寄存器转换成物理寄存器。

### mips 代码设计

翻译过程和 visitor 的设计类似，为不同的 ir 指令设计了不同的方法，也为不同种类的 mips 指令设计了不同的类，方便分别处理和优化。

mips 相关的类设计时使用了 `std::unique_ptr` 表示所有权，避免了内存释放相关问题。对于那些不具有所有权的指针，则使用了裸指针。

mips 代码主要分为 3 个部分：
- 可以用作操作数的 `operand`
- 保存信息和结构的 `component`
- 代表指令的 `instruction`

### 寄存器分配设计

对于不开启优化的寄存器分配，我采用了简单的贪心算法：
1. 计算 Block 的 liveIn 和 liveOut，根据此建立冲突图。（没有使用更好的定义链分析建图）
2. 随后在图上进行 DFS 染色，遇到无法染色的节点则分配内存，可以染色则分配寄存器。（没有进行更严格的图着色的优化）
3. 分配好跨 Block 使用的虚拟寄存器后，再进行 Block 内的寄存器分配。
4. 局部寄存器分配方案则更加暴力，对于遍历到的暂未分配的虚拟寄存器，直接分配一个物理寄存器（如果有），否则分配内存。当然如果这是一个虚拟寄存器在 Block 中的最后一次使用，则可以释放其占有的物理寄存器资源。
