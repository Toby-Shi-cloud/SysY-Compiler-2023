#include <iostream>
#include "frontend/lexer.h"

int main() {
    std::string src, s;
    while (std::getline(std::cin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    for (auto token : lexer) {
        dbg(token);
        std::cout << token << std::endl;
    }
    return 0;
}
