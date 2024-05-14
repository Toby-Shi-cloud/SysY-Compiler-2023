#include <fstream>
#include <iostream>
#include "settings.h"
#include "frontend/parser.h"
#include "frontend/visitor.h"
#include "mir/manager.h"
#include "mips/component.h"
#include "backend/translator.h"

frontend::message_queue_t message_queue;
mir::Manager mir_manager;
mips::Module mips_module;

bool no_mips = false;
const char *source_file = "testfile.sy";
const char *llvm_ir_file = "testfile.ll";
const char *mips_file = "testfile.asm";

int main(int argc, char **argv) {
    set_optimize_level(2); // as default
    decltype(source_file) *ptr = &source_file;
    for (int i = 1; i < argc; i++) {
        using namespace std::string_literals;
        if (argv[i] == "-O0"s) set_optimize_level(0);
        else if (argv[i] == "-O2"s) set_optimize_level(2);
        else if (argv[i] == "-no-S"s) no_mips = true;
        else if (argv[i] == "-S"s) ptr = &mips_file;
        else if (argv[i] == "-emit-llvm"s) ptr = &llvm_ir_file;
        else *ptr = argv[i], ptr = &source_file;
    }

    std::ifstream fin(source_file);
    std::ofstream fir(llvm_ir_file);
    std::ofstream fmips(mips_file);

    std::string src, s;
    while (std::getline(fin, s)) {
        src += s + '\n';
    }
    frontend::lexer::Lexer lexer(src);
    frontend::parser::SysYParser parser(lexer, message_queue);
    parser.parse();
    dbg(parser.comp_unit());
    frontend::visitor::SysYVisitor visitor(mir_manager, message_queue);
    visitor.visit(parser.comp_unit());

    sort_by_line(message_queue);
    for (auto &message: message_queue) {
        std::cout << message << std::endl;
    }
    if (!message_queue.empty()) return 0;

    mir_manager.optimize();
    mir_manager.allocName();
    mir_manager.output(fir);

    if (no_mips) return 0;

    backend::Translator translator(&mir_manager, &mips_module);
    translator.translate();

    if (opt_settings.using_inline_printer)
        inline_printer(fmips, mips_module);
    else fmips << mips_module;
    return 0;
}
