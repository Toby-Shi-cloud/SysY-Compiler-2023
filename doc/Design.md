# SysY 2023~2024 编译器设计文档

本项目地址：https://github.com/Toby-Shi-cloud/SysY-Compiler-2023
SysY Compiler Project 2023 by Toby Shi.

## 第三方依赖

- "clipp" 用于命令行参数解析
- "dbg.h" 用于 debug
- "magic_enum" 用于枚举反射

## 编译器总体设计

### 总体结构

参考 LLVM 的设计，本编译器也计划分为以下几个子项目：

1. 前端 - 负责将 SysY 语言 (C 语言子集) 翻译成 IR。为了方便测试，我采用了 LLVM IR 作为中间表示。
2. 优化器 - 负责对 IR 进行各种优化。
3. 后端 - 负责将 IR 翻译成目标机器的汇编代码 (riscv-64gc)。

### 文件组织

本编译器的源代码文件结构如下：

```
.
├── src
│   ├── backend // 体系架构无关的后端框架
│   │   ├── translator.h
│   │   └── ...
│   ├── frontend // 前端
│   │   ├── lexer.h
│   │   ├── parser.h
│   │   ├── visitor.h
│   │   └── ...
│   ├── mips // mips 后端，不过该后端目前仅停留在无浮点数的版本
│   │   └── ...
│   ├── mir // 中间代码
│   │   ├── type.h
│   │   ├── value.h
│   │   ├── derived_value.h
│   │   ├── instruction.h
│   │   └── ...
│   ├── opt // 中端优化
│   │   └── ...
│   ├── riscv // riscv 后端，大赛版本
│   │   └── ...
│   ├── main.cpp
│   └── ...
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
    std::optional<Token> next_token_try_operator(); // 尝试解析运算符
    std::optional<Token> next_token_try_number(); // 尝试解析数字
    std::optional<Token> next_token_try_identifier(); // 尝试解析标识符/关键字
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

在上面的架构前提下，我的 Parser 可以直接使用声明式写法，示例如下：

```cpp
// <ReturnStmt> ::= "return" <Exp> ";"
template <>
pGrammarNode SysYParser::parse_impl<ReturnStmt>() {
    auto gen = generator<RETURNTK>() + generator<Exp>() * OPTION + generator<SEMICN>();
    return grammarNode(ReturnStmt, gen);
}
```

## 中间代码生成设计

- namespace `frontend::visitor`
- namespace `mir`

中间代码生成均主要在 visitor 中实现。

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

为了简化中间代码生成的难度，我使用了 LLVM IR 的 opaque pointer (模糊指针)，避免了一些不必要的 `bitcast` 或 `getelementptr` 这样的指针变换和计算指令。

## 代码生成设计

- namespace `backend`
- namespace `mips`

生成最终的 mips 代码的难度其实并不如想象中的那么简单，主要是 LLVM IR 的设计和 mips 的设计并非总是一一对应的，有些部分必须要进行一些转换。
因此我设计的后端先将 LLVM IR 转换成含有虚拟寄存器的 mips 代码，随后再通过寄存器分配将虚拟寄存器转换成物理寄存器。

### 后端代码设计

在架构无关的后端中，主要设计了 `Operand` 及其派生类，以及众多 `Component` 类，包括：

```cpp
// Operands
struct Operand; // 任何操作数，包括常量，寄存器，标签等
struct Register; // 寄存器基类
struct VirRegister; // 虚拟寄存器
struct Label; // 标签
// Components
struct InstructionBase; // 指令基类
struct SubBlock; // 不带标签，中途不跳转的基本块
struct Block; // 带标签，由 SubBlock 组成
struct Function; // 函数
struct GlobalVar; // 全局变量
struct Module; // 整个汇编模块
```

另外设计了一个翻译器基类，用于表达架构无关的基础代码。

### riscv 后端设计

`Operand` 部分额外新增了物理寄存器相关类，立即数相关类。
`Instruction` 则增加了各式各样的 riscv 指令（指令按照 R, I, S, B, U, J 分别派生，另外额外派生常用伪指令类）。
`Translator` 针对所有中端用到的 IR 进行了翻译。

### 寄存器分配设计

目前的版本采用全局图着色算法：

1. 计算 Block 的 liveIn 和 liveOut。
2. 根据 Block 的 liveIn 和 liveOut 依次计算各个指令的 liveIn 和 liveOut。
3. 根据指令的 liveIn 和 liveOut 建立冲突图。（考虑到从 SSA 翻译过来的 mips 指令绝大多数仍然是 SSA，所以不需要使用定义链分析）
4. 使用图着色算法进行寄存器分配。（包括：simplify、coalesce、freeze、spill、select）

## 主要优化

主要做到的优化：

1. mem2reg 将不需要地址的变量尽可能优化到寄存器中。
2. constant-folding 常量折叠。尽可能增加编译器常数计算。
3. gcm/gvn 全局值移动和全局值编号。尽可能减少重复计算，并把不需要在循环中的代码提到循环外。
4. div-mul-opt 尽可能使用移位代替乘除法。将除法转换为乘法。尽可能减少除法指令。
5. function-inline 内联函数。
6. constexpr 尽可能将函数转换为 constexpr 函数（即能在编译期计算的函数要在编译期计算）。
7. array-spilt 将实际不需要使用到数组地址的数组拆分，然后优化到寄存器中。
8. tail-recusive-opt 尾递归优化。尾递归可以优化为循环，减少栈消耗。
