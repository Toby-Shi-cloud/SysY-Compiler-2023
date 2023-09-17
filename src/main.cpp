#include <iostream>
#include "frontend/lexer.h"

int main() {
    freopen("testfile.txt", "r", stdin);
    freopen("output.txt", "w", stdout);
    std::string src, s;
    while (std::getline(std::cin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    for (auto token : lexer) {
        std::cout << dbg(token) << std::endl;
    }
    return 0;
}
