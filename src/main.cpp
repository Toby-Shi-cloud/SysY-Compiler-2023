#include <fstream>
#include <iostream>
#include "frontend/lexer.h"

int main() {
    std::ifstream fin("testfile.txt");
    std::ofstream fout("testfile.out");
    std::string src, s;
    while (std::getline(fin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    for (auto token : lexer) {
        fout << dbg(token) << std::endl;
    }
    return 0;
}
