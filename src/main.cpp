#include <fstream>
#include <iostream>
#include "frontend/message.h"
#include "frontend/parser.h"
#include "frontend/visitor.h"

int main(int argc, char **argv) {
    std::ifstream fin(argc >= 2 ? argv[1] : "testfile.txt");
    frontend::message_queue_t message_queue;

    std::string src, s;
    while (std::getline(fin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    frontend::parser::SysYParser parser(lexer, message_queue);
    parser.parse();
    frontend::visitor::SysYVisitor visitor(message_queue);
    visitor.visit(parser.comp_unit());

    frontend::sort_by_line(message_queue);
    for (auto &message: message_queue) {
        std::cout << message << std::endl;
    }
    return 0;
}
