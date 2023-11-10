
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
    bool use65c02 = false;
    std::string outFile;
    bool dumpSyms = false;
    bool showUndef = false;
    bool showTrace = false;
    bool noScreen = false;
    bool quiet = false;
    bool doRun = false;
    bool traceCode = false;
    int maxPasses = 10;
    std::string listFile;
    std::string symbolFile;
    std::string programFile;
    OutFmt outFmt = OutFmt::Prg;
    bool compress = false;
    bool astCache = true;
    int32_t start = -1;

    void parseArgs(int argc, char** argv)
    {
        static const std::map<std::string, OutFmt> tr{
            {"raw"s, OutFmt::Raw},
            {"prg"s, OutFmt::Prg},
            {"crt"s, OutFmt::Crt},
        };

        bool noCache = false;
        CLI::App app{"badass"};
        app.set_help_flag();
        auto* help = app.add_flag("-h,--help", "Request help");
        app.add_option("-f,--format", outFmt, "Output format")
            ->transform(CLI::CheckedTransformer(tr, CLI::ignore_case));
        app.add_flag("--trace", showTrace, "Trace rule invocations");
        app.add_flag("--run", doRun, "Run program");
        app.add_option("--max-passes", maxPasses, "Max assembler passes");
        app.add_flag("--show-undefined", showUndef,
                     "Show undefined after each pass");
        app.add_flag("-q,--quiet", quiet, "Less noise");
        app.add_option("--org", start, "Set default start address");
        app.add_flag("-c,--compress", compress, "Compress program");
        app.add_flag("--no-cache", noCache, "Don't cache generated ASTs");
        app.add_flag("--no-screen", noScreen, "Don't use textmode graphics in emulator");
        app.add_flag("--trace-code", traceCode, "Trace executed assembly");

        app.add_flag("-S,--symbols", dumpSyms, "Dump symbol table");
        app.add_option("-l,--list-file", listFile, "Output assembly listing");
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
            astCache = !noCache;
            if (compress) {
                outFmt = OutFmt::PackedPrg;
            }

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
    }

    void setupAssembler(Assembler& assem)
    {
        assem.setMaxPasses(maxPasses);
        assem.setDebugFlags((showUndef ? Assembler::DEB_PASS : 0) |
                            (showTrace ? Assembler::DEB_TRACE : 0));

        assem.useCache(!doRun && astCache);

        if (outFile.empty()) {
            outFile =
                outFmt == OutFmt::Prg
                    ? "result.prg"
                    : (outFmt == OutFmt::Crt ? "result.crt" : "result.bin");
        }
        auto& mach = assem.getMachine();
        auto& syms = assem.getSymbols();
        mach.setCpu(use65c02 ? Machine::CPU::CPU_65C02 : Machine::CPU_6502);

        mach.SetTracing(traceCode);

        if (start >= 0) {
            auto& section = mach.getSection("default");
            section.start = start;
        }

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
//        int pc = 0;
//        for(auto&& info : assem.getLines()) {
//            if (info.second > 0) {
//                fmt::print("{:04x} {}:{}\n", pc, info.first, info.second);
//            }
//            pc++;
//        }
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

        Pet100 emu{assem.getMachine(), state.noScreen};

        if (!state.programFile.empty()) {
            const utils::File f{state.programFile};
            uint16_t start = f.read<uint8_t>();
            start |= (f.read<uint8_t>() << 8);
            std::vector<uint8_t> data(f.getSize() - 2);
            f.read(data.data(), data.size());
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
        times.reserve(state.sourceFiles.size());
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
                i++;
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


    std::unordered_map<std::string, std::vector<std::string>> files;
    uint32_t pc = 0;
    for (auto&& [file, line] : assem.getLines())
    {
        if (!file.empty()) {
            if (!files.contains(file)) {
                utils::File f {file};
                auto contents = f.readAllString();
                std::vector<std::string> split = utils::split(contents, "\n");
                files[file] = split;
            }
            auto& contents = files[file];
            fmt::print("{:04x}: {}\n", pc, contents[line-1]);
        }
        pc++;
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
        mach.sortSectionsByStart();
        for (auto const& section : mach.getSections()) {
            if (!section.data.empty()) {
                fmt::print("{:04x}-{:04x} {}\n", section.start,
                           section.start + section.data.size()-1, section.name);
            }
        }
    }

    if (state.dumpSyms) assem.printSymbols();
    if (!state.symbolFile.empty()) {
        assem.writeSymbols(fs::path{state.symbolFile});
    }

    return 0;
}
