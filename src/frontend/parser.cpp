//
// Created by toby on 2023/9/22.
//

#include "parser.h"

// SysYParser parse_impl
namespace frontend::parser {
    using namespace grammar::grammar_type;
    using namespace lexer::token_type;

    template<>
    pGrammarNode SysYParser::parse_impl<CompUnit>() {
        auto gen = (generator<FuncDef>() | generator<Decl>()) * MANY;
        auto result = grammarNode(CompUnit, gen);
        if (current != tokens.end())
            throw (dbg(*result, *current), std::runtime_error("Error while parsing CompUnit"));
        return result;
    }

    template<>
    pGrammarNode SysYParser::parse_impl<Decl>() {
        auto gen = generator<ConstDecl>() | generator<VarDecl>();
        return grammarNode(Decl, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ConstDecl>() {
        auto gen = generator<CONSTTK>() + generator<BType>() + generator<ConstDef>() +
                   (generator<COMMA>() + generator<ConstDef>()) * MANY +
                   generator<SEMICN>('i');
        return grammarNode(ConstDecl, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<BType>() {
        auto gen = generator<INTTK>() | generator<FLOATTK>();
        return grammarNode(BType, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ConstDef>() {
        auto gen = generator<IDENFR>() + (generator<LBRACK>() + generator<ConstExp>() + generator<RBRACK>('k')) * MANY +
                   generator<ASSIGN>() + generator<ConstInitVal>();
        return grammarNode(ConstDef, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ConstInitVal>() {
        auto gen = generator<ConstExp>() |
                   generator<LBRACE>() + generator<ConstInitVal>() +
                   (generator<COMMA>() + generator<ConstInitVal>()) * MANY +
                   generator<RBRACE>();
        return grammarNode(ConstInitVal, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<VarDecl>() {
        auto gen = generator<BType>() + generator<VarDef>() +
                   (generator<COMMA>() + generator<VarDef>()) * MANY +
                   generator<SEMICN>('i');
        return grammarNode(VarDecl, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<VarDef>() {
        auto gen = generator<IDENFR>() + (generator<LBRACK>() + generator<ConstExp>() + generator<RBRACK>('k')) * MANY +
                   (generator<ASSIGN>() + generator<InitVal>()) * OPTION;
        return grammarNode(VarDef, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<InitVal>() {
        auto gen = generator<Exp>() |
                   generator<LBRACE>() + generator<InitVal>() +
                   (generator<COMMA>() + generator<InitVal>()) * MANY +
                   generator<RBRACE>();
        return grammarNode(InitVal, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<FuncDef>() {
        auto gen = generator<FuncType>() + generator<IDENFR>() + generator<LPARENT>() +
                   generator<FuncFParams>() * OPTION + generator<RPARENT>('j') + generator<Block>();
        return grammarNode(FuncDef, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<FuncType>() {
        auto gen = generator<VOIDTK>() | generator<INTTK>() | generator<FLOATTK>();
        return grammarNode(FuncType, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<FuncFParams>() {
        auto gen = generator<FuncFParam>() + (generator<COMMA>() + generator<FuncFParam>()) * MANY;
        return grammarNode(FuncFParams, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<FuncFParam>() {
        auto gen = generator<BType>() + generator<IDENFR>() +
                   (generator<LBRACK>() + generator<RBRACK>('k') +
                    (generator<LBRACK>() + generator<ConstExp>() + generator<RBRACK>('k')) * MANY) * OPTION;
        return grammarNode(FuncFParam, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<Block>() {
        auto gen = generator<LBRACE>() + generator<BlockItem>() * MANY + generator<RBRACE>();
        return grammarNode(Block, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<BlockItem>() {
        auto gen = generator<Decl>() | generator<Stmt>();
        return grammarNode(BlockItem, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<Stmt>() {
        auto gen = generator<AssignStmt>() | generator<BlockStmt>() | generator<IfStmt>() |
                   generator<WhileStmt>() | generator<ForLoopStmt>() | generator<BreakStmt>() |
                   generator<ContinueStmt>() | generator<ReturnStmt>() | generator<ExpStmt>();
        return grammarNode(Stmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<AssignStmt>() {
        auto gen = generator<LVal>() + generator<ASSIGN>() + generator<Exp>() + generator<SEMICN>('i');
        return grammarNode(AssignStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ExpStmt>() {
        auto gen = generator<SEMICN>() | generator<Exp>() + generator<SEMICN>('i');
        return grammarNode(ExpStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<BlockStmt>() {
        auto gen = generator<Block>();
        return grammarNode(BlockStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<IfStmt>() {
        auto gen = generator<IFTK>() + generator<LPARENT>() + generator<Cond>() + generator<RPARENT>('j') +
                   generator<Stmt>() + (generator<ELSETK>() + generator<Stmt>()) * OPTION;
        return grammarNode(IfStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<WhileStmt>() {
        auto gen = generator<WHILETK>() + generator<LPARENT>() + generator<Cond>() + generator<RPARENT>() +
                   generator<Stmt>();
        return grammarNode(WhileStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ForLoopStmt>() {
        auto gen = generator<FORTK>() + generator<LPARENT>() + generator<ForStmt>() * OPTION + generator<SEMICN>() +
                   generator<Cond>() * OPTION + generator<SEMICN>() + generator<ForStmt>() * OPTION +
                   generator<RPARENT>('j') + generator<Stmt>();
        return grammarNode(ForLoopStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<BreakStmt>() {
        auto gen = generator<BREAKTK>() + generator<SEMICN>('i');
        return grammarNode(BreakStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ContinueStmt>() {
        auto gen = generator<CONTINUETK>() + generator<SEMICN>('i');
        return grammarNode(ContinueStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ReturnStmt>() {
        auto gen = generator<RETURNTK>() + generator<Exp>() * OPTION + generator<SEMICN>('i');
        return grammarNode(ReturnStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ForStmt>() {
        auto gen = generator<LVal>() + generator<ASSIGN>() + generator<Exp>();
        return grammarNode(ForStmt, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<Exp>() {
        auto gen = generator<AddExp>();
        return grammarNode(Exp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<Cond>() {
        auto gen = generator<LOrExp>();
        return grammarNode(Cond, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<LVal>() {
        auto gen = generator<IDENFR>() + (generator<LBRACK>() + generator<Exp>() + generator<RBRACK>('k')) * MANY;
        return grammarNode(LVal, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<PrimaryExp>() {
        auto gen = generator<LPARENT>() + generator<Exp>() + generator<RPARENT>('j') |
                   generator<LVal>() | generator<Number>();
        return grammarNode(PrimaryExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<Number>() {
        auto gen = generator<INTCON>() | generator<FLOATCON>();
        return grammarNode(Number, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<UnaryExp>() {
        auto gen = generator<IDENFR>() + generator<LPARENT>() + generator<FuncRParams>() * OPTION +
                   generator<RPARENT>('j') | generator<PrimaryExp>() | generator<UnaryOp>() + generator<UnaryExp>();
        return grammarNode(UnaryExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<UnaryOp>() {
        auto gen = generator<PLUS>() | generator<MINU>() | generator<NOT>();
        return grammarNode(UnaryOp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<FuncRParams>() {
        auto gen = generator<Exp>() + (generator<COMMA>() + generator<Exp>()) * MANY;
        return grammarNode(FuncRParams, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<MulExp>() {
        auto gen = generator<UnaryExp>() +
                   ((generator<MULT>() | generator<DIV>() | generator<MOD>()) + generator<UnaryExp>()) * MANY;
        return grammarNode(MulExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<AddExp>() {
        auto gen = generator<MulExp>() +
                   ((generator<PLUS>() | generator<MINU>()) + generator<MulExp>()) * MANY;
        return grammarNode(AddExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<RelExp>() {
        auto gen = generator<AddExp>() +
                   ((generator<LSS>() | generator<LEQ>() | generator<GRE>() | generator<GEQ>()) + generator<AddExp>()) *
                   MANY;
        return grammarNode(RelExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<EqExp>() {
        auto gen = generator<RelExp>() +
                   ((generator<EQL>() | generator<NEQ>()) + generator<RelExp>()) * MANY;
        return grammarNode(EqExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<LAndExp>() {
        auto gen = generator<EqExp>() + (generator<AND>() + generator<EqExp>()) * MANY;
        return grammarNode(LAndExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<LOrExp>() {
        auto gen = generator<LAndExp>() + (generator<OR>() + generator<LAndExp>()) * MANY;
        return grammarNode(LOrExp, gen);
    }

    template<>
    pGrammarNode SysYParser::parse_impl<ConstExp>() {
        auto gen = generator<AddExp>();
        return grammarNode(ConstExp, gen);
    }
}

// SysYParser other methods
namespace frontend::parser {
    pTerminalNode SysYParser::need_terminal(lexer::token_type_t type, int error_code) {
        if (current->type == type)
            return std::make_unique<TerminalNode>(*current++);
        if (error_code) {
            using namespace std::string_literals;
            auto last_token = current - 1;
            auto line = last_token->line;
            auto column = last_token->column + last_token->raw.size();
            message_queue.push_back(
                {
                    message::ERROR, error_code, line, column,
                    "missing token '"s + raw[type] + "'"
                });
            return std::make_unique<TerminalNode>(lexer::Token{type, "", line, column});
        }
        return nullptr;
    }

    void SysYParser::parse() {
        if (_comp_unit) return;
        _comp_unit = parse_impl<CompUnit>();
    }
}

// generator_t operator
namespace frontend::parser {
    generator_t operator+(const generator_t &one, const generator_t &other) {
        return [one, other](SysYParser *self) -> optGrammarNodeList {
            auto backup = self->current;
            auto result = one(self);
            if (!result) return std::nullopt;
            if (auto other_result = other(self)) {
                result->splice(result->end(), other_result.value());
                return result;
            }
            self->current = backup;
            return std::nullopt;
        };
    }

    generator_t operator|(const generator_t &one, const generator_t &other) {
        return [one, other](SysYParser *self) -> optGrammarNodeList {
            if (auto result = one(self)) return result;
            if (auto result = other(self)) return result;
            return std::nullopt;
        };
    }

    generator_t operator*(const generator_t &gen, _option) {
        return [gen](SysYParser *self) -> optGrammarNodeList {
            if (auto result = gen(self))
                return result;
            return pGrammarNodeList{};
        };
    }

    generator_t operator*(const generator_t &gen, _many) {
        return [gen](SysYParser *self) -> optGrammarNodeList {
            pGrammarNodeList result{};
            while (auto child = gen(self)) {
                result.splice(result.end(), child.value());
            }
            return result;
        };
    }
}
