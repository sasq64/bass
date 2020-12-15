
#include "assembler.h"
#include "defines.h"
#include "machine.h"
#include "pet100.h"

#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/split.h>

#include <CLI/CLI.hpp>

#include <cstdio>
#ifndef _WIN32
#    include <csignal>
#endif

#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using namespace std::string_literals;

static const char* const banner = R"(
 _               _
| |__   __ _  __| | __ _ ___ ___
| '_ \ / _` |/ _` |/ _` / __/ __|
| |_) | (_| | (_| | (_| \__ \__ \
|_.__/ \__,_|\__,_|\__,_|___/___/
6502 assembler (1.0rc1)      /sasq
)";

struct AssemblerState
{
    std::vector<std::string> sourceFiles;
    std::vector<std::string> scriptFiles;
    std::vector<std::string> definitions;
    std::string outFile;
    bool dumpSyms = false;
    bool showUndef = false;
    bool showTrace = false;
    bool quiet = false;
    bool doRun = false;
    int maxPasses = 10;
    std::string listFile;
    std::string symbolFile;
    std::string programFile;
    OutFmt outFmt = OutFmt::Prg;
    sixfive::CPU cpu = sixfive::CPU::Cpu6502;

    void parseArgs(int argc, char** argv)
    {
        bool use65c02 = false;
        static const std::map<std::string, sixfive::CPU> cpuTR{
            {"6502"s, sixfive::CPU::Cpu6502},
            {"65c02"s, sixfive::CPU::Cpu65C02},
            {"4510"s, sixfive::CPU::Cpu4510},
        };

        static const std::map<std::string, OutFmt> formatTR{
            {"raw"s, OutFmt::Raw},
            {"prg"s, OutFmt::Prg},
            {"crt"s, OutFmt::Crt},
        };

        CLI::App app{"badass"};
        app.set_help_flag();
        auto* help = app.add_flag("-h,--help", "Request help");
        app.add_option("-f,--format", outFmt, "Output format")
            ->transform(CLI::CheckedTransformer(formatTR, CLI::ignore_case));
        app.add_flag("--trace", showTrace, "Trace rule invocations");
        app.add_flag("--run", doRun, "Run program");
        app.add_option("--max-passes", maxPasses, "Max assembler passes");
        app.add_flag("--show-undefined", showUndef,
                     "Show undefined after each pass");
        app.add_flag("-q,--quiet", quiet, "Less noise");
        app.add_flag("-S,--symbols", dumpSyms, "Dump symbol table");
        app.add_option("-l,--list-file", listFile, "Output assembly listing");
        app.add_option("--cpu", cpu, "CPU")
            ->transform(CLI::CheckedTransformer(cpuTR, CLI::ignore_case));
        app.add_flag("--65c02", use65c02, "Target 65c02");
        app.add_option("-s,--table", symbolFile,
                       "Write numeric symbols to file");
        app.add_option("-o,--out", outFile, "Output file");
        app.add_option("-p,--prg", programFile, "Program file");
        app.add_option("-D", definitions, "Add symbol");
        app.add_option("-i", sourceFiles, "Sources to compile");
        app.add_option("-x,--lua", scriptFiles, "LUA scripts to load")
            ->check(CLI::ExistingFile);
        app.add_option("source", sourceFiles, "Sources to compile")
            ->check(CLI::ExistingFile);

        bool showHelp = false;

        try {
            app.parse(argc, argv);
            showHelp = (*help || (sourceFiles.empty() && scriptFiles.empty() &&
                                  programFile.empty()));
            if (showHelp) {
                if (!quiet) puts(banner + 1);

                throw CLI::CallForHelp();
            }
        } catch (const CLI::ParseError& e) {
            app.exit(e);
        }
        if (showHelp) exit(0);

        if (use65c02) {
            cpu = sixfive::CPU::Cpu65C02;
        }
    }

    void setupAssembler(Assembler& assem)
    {
        assem.setMaxPasses(maxPasses);
        assem.setDebugFlags((showUndef ? Assembler::DEB_PASS : 0) |
                            (showTrace ? Assembler::DEB_TRACE : 0));

        assem.useCache(!doRun);

        if (outFile.empty()) {
            outFile =
                outFmt == OutFmt::Prg
                    ? "result.prg"
                    : (outFmt == OutFmt::Crt ? "result.crt" : "result.bin");
        }
        auto& mach = assem.getMachine();
        auto& syms = assem.getSymbols();
        mach.setCpu(cpu);

        for (auto const& sf : scriptFiles) {
            assem.addScript(fs::path(sf));
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
    }

    bool assemble(Assembler& assem)
    {
        bool failed = false;
        for (auto const& sourceFile : sourceFiles) {
            auto sp = fs::path(sourceFile);
            if (!assem.parse_path(sp)) {
                for (auto const& e : assem.getErrors()) {
                    if (e.level == ErrLevel::Error) failed = true;
                    fmt::print("{}:{}: {}: {}\n", e.file, e.line,
                               e.level == ErrLevel::Warning ? "warning"
                                                            : "error",
                               e.message.c_str());
                }
            }
        }
        return !failed;
    }
};

int main(int argc, char** argv)
{
    AssemblerState state;
    state.parseArgs(argc, argv);

    Assembler assem;
    state.setupAssembler(assem);

    auto& mach = assem.getMachine();

#ifndef _WIN32
    struct sigaction sh = {};
    sh.sa_handler = [](int) {
        // Restore cursor and exit alt mode
        fputs("\x1b[?25h", stdout);
        fputs("\x1b[?1049l", stdout);
        exit(0);
    };
    sigemptyset(&sh.sa_mask);
    sh.sa_flags = 0;
    sigaction(SIGINT, &sh, nullptr);
#endif

    while (state.doRun) {

        Pet100 emu;

        if (!state.programFile.empty()) {
            utils::File f{state.programFile};
            uint16_t start = f.read<uint8_t>();
            start |= (f.read<uint8_t>() << 8);
            std::vector<uint8_t> data(f.getSize() - 2);
            f.read(&data[0], data.size());
            emu.load(start, data);
            emu.run(start);
            exit(0);
        }

        assem.clear();
        assem.getSymbols().set("CONSOLE_WIDTH",
                               static_cast<Number>(emu.get_width()));
        assem.getSymbols().set("CONSOLE_HEIGHT",
                               static_cast<Number>(emu.get_height()));

        if (!state.assemble(assem)) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        std::vector<fs::file_time_type> times;
        for (auto const& sourceFile : state.sourceFiles) {
            times.push_back(fs::last_write_time(sourceFile));
        }
        int32_t start = 0x10000;
        for (auto const& s : mach.getSections()) {
            if (!s.data.empty() && (s.flags & NoStorage) == 0) {
                emu.load(s.start, s.data);
                if (s.start < start) {
                    start = s.start;
                }
            }
        }
        emu.start(start);
        bool quit = false;
        bool recompile = false;
        while (!quit) {
            quit = emu.update();
            size_t i = 0;
            for (auto const& sourceFile : state.sourceFiles) {
                auto lastTime = times[i];
                try {
                    auto thisTime = fs::last_write_time(sourceFile);
                    times[i] = thisTime;
                    if (thisTime != lastTime) {
                        recompile = true;
                        quit = true;
                        break;
                    }
                } catch (fs::filesystem_error&) {
                }
            }
        }
        if (recompile) {
            continue;
        }
        return 0;
    }

    assem.clear();
    if (!state.assemble(assem)) {
        return 1;
    }

    try {
        mach.write(state.outFile, state.outFmt);
    } catch (utils::io_exception&) {
        fmt::print(stderr, "**Error: Could not write output file {}\n",
                   state.outFile);
        return 1;
    }

    if (!state.listFile.empty()) {
        mach.writeListFile(state.listFile);
    }

    if (!state.quiet) {
        for (auto const& section : mach.getSections()) {
            if (!section.data.empty()) {
                fmt::print("{:04x}-{:04x} {}\n", section.start,
                           section.start + section.data.size(), section.name);
            }
        }
    }

    if (state.dumpSyms) assem.printSymbols();
    if (!state.symbolFile.empty()) {
        assem.writeSymbols(fs::path{state.symbolFile});
    }

    return 0;
}
