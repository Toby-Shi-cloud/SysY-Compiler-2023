#include <fstream>
#include <iostream>
#include "settings.h"
#include "frontend/parser.h"
#include "frontend/visitor.h"
#include "mir/manager.h"
#include "mips/component.h"
#include "backend/translator.h"

#include <clipp.h>
using namespace clipp;

int main(int argc, char **argv) {
    int opt_level = 1;
    bool llvm_ir = false, assembly = false, help = false;
    std::string infile, outfile = "a.out";

    auto cli = (
        opt_value("source", infile).required(true)
            .if_missing([]{ std::cout << "Source file not provided!\n"; } )
            .if_repeated([argv](int idx){ std::cout << "Duplicate source file: " << argv[idx] << "\n"; } ),
        option("-O").doc("optimization level (0-3)") & value("level", opt_level),
        option("-emit-llvm").set(llvm_ir).doc("emit llvm ir"),
        option("-S").set(assembly).doc("emit assembly"),
        option("-o").doc("output file (default a.out)") & value("output", outfile)
    );
    auto cli_help = option("-h", "--help").set(help).doc("show help");

    if (!parse(argc, argv, cli | cli_help)) {
        std::cout << "Usage:" << usage_lines(cli, argv[0]);
        return 1;
    }

    if (help) {
        std::cout << make_man_page(cli, argv[0]);
        return 0;
    }

    if (!llvm_ir && !assembly) {
        std::cout << "Error: no output specified" << std::endl;
        return 1;
    }

    std::ifstream fin(infile);
    if (!fin) {
        std::cerr << "Error: cannot open file " << infile << std::endl;
        return 1;
    }

    frontend::message_queue_t message_queue;
    mir::Manager mir_manager;
    mips::Module mips_module;

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
    if (llvm_ir) {
        std::ofstream fir(assembly ? outfile + ".ll" : outfile);
        mir_manager.output(fir);
    }

    if (!assembly) return 0;

    std::ofstream fmips(outfile);
    backend::Translator translator(&mir_manager, &mips_module);
    translator.translate();

    if (opt_settings.using_inline_printer)
        inline_printer(fmips, mips_module);
    else fmips << mips_module;
    return 0;
}
