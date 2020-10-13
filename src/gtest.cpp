
extern char const* const grammar6502;

#include <coreutils/file.h>

#include <peglib.h>
#include <iostream>

int main(int argc, char** argv)
{
    peg::parser p{grammar6502};

    printf("Parsing\n");

    utils::File f{argv[1]};

    auto data = f.readAllString();
    p.log = [](size_t line, size_t col, const std::string& msg) {
        std::cerr << line << ":" << col << ": " << msg << "\n";
    };
    auto rc = p.parse(data.c_str());
    printf("Result %d\n", (int)rc);

    return 0;
}
