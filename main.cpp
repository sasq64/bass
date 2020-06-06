
#include "assembler.h"
#include "defines.h"
#include "machine.h"

#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/path.h>
#include <coreutils/split.h>

#include <CLI/CLI.hpp>

const char* banner = R"( _               _
| |__   __ _  __| | __ _ ___ ___
| '_ \ / _` |/ _` |/ _` / __/ __|
| |_) | (_| | (_| | (_| \__ \__ \
|_.__/ \__,_|\__,_|\__,_|___/___/
6502 assembler (beta)      /sasq
)";

int main(int argc, char** argv)
{
    using std::any_cast;
    std::vector<std::string> sourceFiles;
    std::vector<std::string> scriptFiles;
    std::vector<std::string> definitions;
    std::string outFile = "result.prg";
    bool raw = false;
    bool prg = false;
    bool dumpSyms = false;
    bool showOpcodes = false;
    puts(banner);

    CLI::App app{"badass"};
    app.add_flag("--ops", showOpcodes, "Show generated opcodes");
    app.add_flag("-p,--prg", prg, "Output PRG");
    app.add_flag("-b,--bin", raw, "Output RAW");
    app.add_flag("-S,--symbols", dumpSyms, "Dump symbol table");
    app.add_option("-o,--out", outFile, "Output file");
    app.add_option("-D", definitions, "Add symbol");
    app.add_option("-i", sourceFiles, "Sources to compile");
    app.add_option("-x,--lua", scriptFiles, "LUA scripts to load")
        ->check(CLI::ExistingFile);
    app.add_option("source", sourceFiles, "Sources to compile")
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    Assembler ass;

    OutFmt outFmt = raw ? OutFmt::Raw : OutFmt::Prg;

    auto& mach = ass.getMachine();
    auto& syms = ass.getSymbols();

    utils::File defFile{"out.def", utils::File::Write};
    mach.setOutput(defFile.filePointer());

    for (auto const& sf : scriptFiles) {
        ass.addScript(utils::path(sf));
    }

    for (auto const& d : definitions) {
        auto parts = utils::split(d, "=");
        if (parts.size() == 1) {
            syms.at<Number>(parts[0]) = 1;
        } else {
            syms.at<Number>(parts[0]) = std::strtod(parts[1], nullptr);
        }
    }

    logging::setLevel(logging::Level::Info);
    for (auto const& sourceFile : sourceFiles) {
        auto sp = utils::path(sourceFile);
        if (!ass.parse_path(sp)) {
            for (auto const& e : ass.getErrors()) {
                fmt::print("{}:{}:{}: {}: {}\n", sourceFile, e.line, e.column,
                           e.level == ErrLevel::Warning ? "warning" : "error",
                           e.message.c_str());
            }
        }
    }
    mach.write(outFile, outFmt);

    for (auto const& section : mach.getSections()) {
        fmt::print("{:04x}-{:04x} {}\n", section.start,
                   section.start + section.data.size(), section.name);
    }

    if (dumpSyms) ass.printSymbols();

    return 0;
}
