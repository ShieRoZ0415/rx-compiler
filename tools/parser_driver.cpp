#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "antlr4-runtime.h"
#include "RxLexer.h"
#include "RxParser.h"

int main(int argc, char** argv) {
    std::string entry = "crate";
    std::string path;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--entry" && i + 1 < argc) {
            entry = argv[++i];
        } else if (!a.empty() && a[0] != '-') {
            path = a;
        }
    }

    if (path.empty()) {
        std::cerr << "usage: parser_driver --entry <crate|expression|typeRef|item|letStatement> <file>\n";
        return 2;
    }

    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot open: " << path << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string code = ss.str();

    antlr4::ANTLRInputStream input(code);
    rxgrammar::RxLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    rxgrammar::RxParser parser(&tokens);

    if (entry == "crate") {
        parser.crate();
    } else if (entry == "expression") {
        parser.expression();
    } else if (entry == "typeRef") {
        parser.typeRef();
    } else if (entry == "item") {
        parser.item();
    } else if (entry == "letStatement") {
        parser.letStatement();
    } else {
        std::cerr << "unknown entry: " << entry << "\n";
        return 2;
    }

    return parser.getNumberOfSyntaxErrors() > 0 ? 1 : 0;
}
