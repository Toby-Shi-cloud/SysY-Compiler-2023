#include <fstream>
#include <iostream>
#include "frontend/parser.h"
#include "frontend/visitor.h"
#include "mir/manager.h"
#include "mips/component.h"
#include "backend/translator.h"

frontend::message_queue_t message_queue;
mir::Manager mir_manager;
mips::Module mips_module;

const char *source_file = "testfile.txt";
const char *llvm_ir_file = "llvm_ir.txt";
const char *mips_file = "mips.txt";
const char *error_file = "error.txt";

int main(int argc, char **argv) {
    decltype(source_file) *ptr = &source_file;
    for (int i = 1; i < argc; i++) {
        using namespace std::string_literals;
        if (argv[i] == "-S"s) ptr = &mips_file;
        else if (argv[i] == "-emit-llvm"s) ptr = &llvm_ir_file;
        else *ptr = argv[i], ptr = &source_file;
    }

    std::ifstream fin(source_file);
    std::ofstream fir(llvm_ir_file);
    std::ofstream fmips(mips_file);
    std::ofstream ferr(error_file);

    std::string src, s;
    while (std::getline(fin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    frontend::parser::SysYParser parser(lexer, message_queue);
    parser.parse();
    frontend::visitor::SysYVisitor visitor(mir_manager, message_queue);
    visitor.visit(parser.comp_unit());

    frontend::sort_by_line(message_queue);
    for (auto &message: message_queue) {
        std::cout << message << std::endl;
        ferr << message.line << " " << (char)message.code << std::endl;
    }
    if (!message_queue.empty()) return 0;

    mir_manager.allocName();
    mir_manager.output(fir);

    backend::Translator translator(&mir_manager, &mips_module);
    translator.translate();
    fmips << mips_module;
    return 0;
}
