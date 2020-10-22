
extern char const* const grammar6502;

#include <coreutils/file.h>
#include <coreutils/log.h>

#include <fmt/format.h>

#include <iostream>
#include <peglib.h>

int main(int argc, char** argv)
{
    peg::parser p{grammar6502};
    p.enable_ast();

    printf("Parsing\n");

    utils::File f{argv[1]};

    auto data = f.readAllString();
    p.log = [](size_t line, size_t col, const std::string& msg) {
        std::cerr << line << ":" << col << ": " << msg << "\n";
    };
    std::shared_ptr<peg::Ast> ast;

    std::string spaces{
        "                                                        "};

    std::function<void(const peg::Ast&, int)> eval = [&](const peg::Ast& ast,
                                                         int indent) {
        fmt::print("{}{} ({})\n", spaces.substr(indent), ast.name, ast.token);
        const auto& nodes = ast.nodes;
        for (auto i = 0; i < nodes.size(); i++) {
            eval(*nodes[i], indent - 2);
        }
    };

    p.enable_packrat_parsing();
    auto rc = p.parse(data.c_str(), ast);

    ast = peg::AstOptimizer(true).optimize(ast);
    if (rc) {
        eval(*ast, 54);
    }
    printf("Result %d\n", (int)rc);

    return 0;
}
