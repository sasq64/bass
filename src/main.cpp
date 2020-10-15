
#include "assembler.h"
#include "defines.h"
#include "machine.h"

#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/path.h>
#include <coreutils/split.h>

#include <CLI/CLI.hpp>

static const char* const banner = R"(
 _               _
| |__   __ _  __| | __ _ ___ ___
| '_ \ / _` |/ _` |/ _` / __/ __|
| |_) | (_| | (_| | (_| \__ \__ \
|_.__/ \__,_|\__,_|\__,_|___/___/
6502 assembler (beta5)      /sasq
)";

int main(int argc, char** argv)
{
    using std::any_cast;
    std::vector<std::string> sourceFiles;
    std::vector<std::string> scriptFiles;
    std::vector<std::string> definitions;
    bool use65c02 = false;
    std::string outFile;
    bool dumpSyms = false;
    bool showUndef = false;
    bool showTrace = false;
    bool quiet = false;
    std::string symbolFile;
    OutFmt outFmt = OutFmt::Prg;
    std::map<std::string, OutFmt> tr{
        {"raw"s, OutFmt::Raw},
        {"prg"s, OutFmt::Prg},
        {"crt"s, OutFmt::Crt},
    };

    CLI::App app{"badass"};
    app.set_help_flag();
    auto help = app.add_flag("-h,--help", "Request help");
    app.add_option("-f,--format", outFmt, "Output format")
        ->transform(CLI::CheckedTransformer(tr, CLI::ignore_case));
    app.add_flag("--trace", showTrace, "Trace rule invocations");
    app.add_flag("--show-undefined", showUndef,
                 "Show undefined after each pass");
    app.add_flag("-q,--quiet", quiet, "Less noise");
    app.add_flag("-S,--symbols", dumpSyms, "Dump symbol table");
    app.add_flag("--65c02", use65c02, "Target 65c02");
    app.add_option("-s,--table", symbolFile, "Write numeric symbols to file");
    app.add_option("-o,--out", outFile, "Output file");
    app.add_option("-D", definitions, "Add symbol");
    app.add_option("-i", sourceFiles, "Sources to compile");
    app.add_option("-x,--lua", scriptFiles, "LUA scripts to load")
        ->check(CLI::ExistingFile);
    app.add_option("source", sourceFiles, "Sources to compile")
        ->check(CLI::ExistingFile);

    bool showHelp = false;

    try {
        app.parse(argc, argv);
        showHelp = (*help || (sourceFiles.empty() && scriptFiles.empty()));
        if (showHelp) {
            if(!quiet) puts(banner + 1);

            throw CLI::CallForHelp();
        }
    } catch (const CLI::ParseError& e) {
        app.exit(e);
    }
    if (showHelp) return 0;
    
    Assembler assem;
    assem.setDebugFlags((showUndef ? Assembler::DEB_PASS : 0) |
                        (showTrace ? Assembler::DEB_TRACE : 0));

    if (outFile.empty()) {
        outFile = outFmt == OutFmt::Prg
                      ? "result.prg"
                      : (outFmt == OutFmt::Crt ? "result.crt" : "result.bin");
    }

    auto& mach = assem.getMachine();
    auto& syms = assem.getSymbols();

    mach.setCpu(use65c02 ? Machine::CPU::CPU_65C02 : Machine::CPU_6502);

    utils::File defFile{"out.def", utils::File::Write};
    mach.setOutput(defFile.filePointer());

    for (auto const& sf : scriptFiles) {
        assem.addScript(utils::path(sf));
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
    bool failed = false;
    for (auto const& sourceFile : sourceFiles) {
        auto sp = utils::path(sourceFile);
        if (!assem.parse_path(sp)) {
            for (auto const& e : assem.getErrors()) {
                if (e.level == ErrLevel::Error) failed = true;
                fmt::print("{}:{}: {}: {}\n", sourceFile, e.line,
                           e.level == ErrLevel::Warning ? "warning" : "error",
                           e.message.c_str());
            }
        }
    }
    if (failed) {
        return 1;
    }
    try {
        mach.write(outFile, outFmt);
    } catch (utils::io_exception& e) {
        fmt::print(stderr, "**Error: Could not write output file {}\n",
                   outFile);
        return 1;
    }

    if (!quiet) {
        for (auto const& section : mach.getSections()) {
            if (section.data.size() > 0) {
                fmt::print("{:04x}-{:04x} {}\n", section.start,
                           section.start + section.data.size(), section.name);
            }
        }
    }

    if (dumpSyms) assem.printSymbols();
    if (!symbolFile.empty()) {
        assem.writeSymbols(utils::path{symbolFile});
    }

    return 0;
}
