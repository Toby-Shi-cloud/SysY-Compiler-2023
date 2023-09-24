#include <fstream>
#include <iostream>
#include "frontend/parser.h"
#include "frontend/visitor.h"

int main() {
    std::ifstream fin("testfile.txt");
    std::ofstream fout("output.txt");
    std::string src, s;
    while (std::getline(fin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    frontend::parser::SysYParser parser(lexer);
    parser.parse();
    frontend::visitor::SysYVisitor visitor(fout);
    visitor.visit(parser.comp_unit());
    return 0;
}
