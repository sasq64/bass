#include "assembler.h"
#include "defines.h"
#include "machine.h"

#include <any>
#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/split.h>
#include <coreutils/text.h>
#include <coreutils/utf8.h>
#include <fmt/format.h>
#include <vector>

using namespace std::string_literals;

static std::unordered_map<uint16_t, uint8_t> char_translate;

// Reset the translation of characters to Petscii/screen code
static void resetTranslate()
{
    char_translate.clear();
    for (int i = 0; i < 256; i++) {
        auto c = i;
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 1;
        else if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 0x41;
        char_translate[i] = c;
    }
}

static void evalIf(Assembler& a, bool cond,
                   std::vector<Assembler::Block> const& blocks)
{
    if (blocks.empty()) {
        throw parse_error("Expected block");
    }
    if (cond) {
        a.evaluateBlock(blocks[0].contents, blocks[0].line);
    } else {
        if (blocks.size() == 3) {
            if (std::any_cast<std::string_view>(blocks[1].contents) != "else") {
                throw parse_error("Expected 'else'");
            }
            a.evaluateBlock(blocks[2].contents, blocks[2].line);
        }
    }
}

static Section parseArgs(std::vector<std::any> const& args)
{
    Section result;
    int i = 0;

    if (args.empty()) {
        throw parse_error("Too few arguments");
    }
    for (auto const& arg : args) {
        if (auto const* p =
                std::any_cast<std::pair<std::string_view, std::any>>(&arg)) {
            if (p->first == "name") {
                result.name = std::any_cast<std::string_view>(p->second);
            } else if (p->first == "start") {
                result.start = number<int32_t>(p->second);
            } else if (p->first == "size") {
                result.size = number<int32_t>(p->second);
            } else if (p->first == "in") {
                result.parent = std::any_cast<std::string_view>(p->second);
            } else if (p->first == "pc") {
                result.pc = number<int32_t>(p->second);
            }
        } else {
            if (i == 0) {
                result.name = std::any_cast<std::string_view>(arg);
            } else if (i == 1) {
                result.start = number<uint32_t>(arg);
            } else if (i == 2) {
                auto res = number<uint64_t>(arg);
                if (res >= 0x100000000) {
                    result.flags = res >> 32;
                } else {
                    result.pc = static_cast<uint32_t>(res);
                }
            }
            i++;
        }
    }
    return result;
}

void initMeta(Assembler& assem)
{
    using std::any_cast;
    auto& mach = assem.getMachine();

    resetTranslate();

    static auto is_space = [](char const c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    static auto strip_space = [](std::string_view sv) -> std::string_view {
      while (is_space(sv.front())) {
          sv.remove_prefix(1);
      }
      while (is_space(sv.back())) {
          sv.remove_suffix(1);
      }
      return sv;
    };


    assem.registerMeta("test", [&](auto const& text, auto const& blocks) {
        auto args = assem.evaluateList(text);
        std::string testName;

        for (auto const& v : args) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                testName = *s;
            } else if (auto* n = any_cast<Number>(&v)) {
                assem.testLocation = static_cast<uint32_t>(*n);
                return;
            }
        }
        if (testName.empty()) {
            testName = "test";
        }

        Check(blocks.size() == 1, "Expected block");
        auto contents = blocks[0].contents;
        auto res = assem.runTest(testName, contents);
        assem.getSymbols().set("tests."s + testName, res);
        // LOGI("Set %s %x", testName, number<int32_t>(res["A"]));
    });

    assem.registerMeta("macro", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");
        auto def = assem.evaluateDefinition(text, blocks[0].line);
        assem.defineMacro(def.name, def.args, blocks[0].line,
                          blocks[0].contents);
    });

    assem.registerMeta("define", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");
        auto def = assem.evaluateDefinition(text);
        auto contents = strip_space(blocks[0].contents);
        assem.addDefine(def.name, def.args, blocks[0].line, contents);
    });

    assem.registerMeta("ascii", [&](auto const&, auto const&) {
        resetTranslate();
        for (int i = 0; i < 256; i++) {
            char_translate[i] = i;
        }
    });

    assem.registerMeta("chartrans", [&](auto const& args, auto const&) {
        if (args.empty()) {
            resetTranslate();
            return;
        }
        auto list = assem.evaluateList(args);
        std::wstring text;
        size_t index = 0;
        for (auto const& v : list) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                text = utils::utf8_decode(*s);
                index = 0;
            } else {
                Check(!text.empty(),
                      "First argument must be a (non-empty) string");
                Check(index < text.length(), "Arguments exceed string length");
                auto n = number<uint8_t>(v);
                auto c = text[index++];
                LOGD("%x %x", n, (uint16_t)c);
                char_translate[c] = n;
            }
        }
    });

    assem.registerMeta("text", [&](auto const& text, auto const&) {
        auto list = assem.evaluateList(text);
        for (auto const& v : list) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                auto ws = utils::utf8_decode(*s);
                for (auto c : ws) {
                    auto b = char_translate.at(c);
                    mach.writeChar(b);
                }
            } else {
                throw parse_error("Need text");
            }
        }
    });

    assem.registerMeta("byte", [&](auto const& text, auto const&) {
        auto list = assem.evaluateList(text);
        for (auto const& v : list) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                for (auto c : *s) {
                    mach.writeChar(c);
                }
            } else {
                auto b = number<uint8_t>(v);
                mach.writeByte(b);
            }
        }
    });

    assem.registerMeta("byte3", [&](auto const& text, auto const&) {
        auto list = assem.evaluateList(text);
        for (auto const& v : list) {
            auto b = number<uint32_t>(v);
            mach.writeByte((b >> 16) & 0xff);
            mach.writeByte((b >> 8) & 0xff);
            mach.writeByte(b & 0xff);
        }
    });

    assem.registerMeta("word", [&](auto const& text, auto const&) {
        auto list = assem.evaluateList(text);
        for (auto const& v : list) {
            auto w = number<int32_t>(v);
            mach.writeByte(w & 0xff);
            mach.writeByte(w >> 8);
        }
    });

    assem.registerMeta("assert", [&](auto const& text, auto const&) {
        if (!assem.isFinalPass()) return;
        auto args = assem.evaluateList(text);
        auto v = any_cast<Number>(args[0]);
        if (v == 0.0) {
            std::string_view msg = text;
            if (args.size() > 1) {
                msg = any_cast<std::string_view>(args[1]);
            }
            throw assert_error(std::string(msg));
        }
    });

    assem.registerMeta("fill", [&](auto const& text, auto const& blocks) {
        std::any data = assem.evaluateExpression(text);

        auto& syms = assem.getSymbols();

        if (auto* vec = any_cast<std::vector<uint8_t>>(&data)) {
            for (size_t i = 0; i < vec->size(); i++) {
                uint8_t d = (*vec)[i];
                syms.erase("i");
                syms.set("i", any_num(i));
                for (Assembler::Block const& b : blocks) {
                    auto contents = strip_space(b.contents);
                    assem.getSymbols().at<Number>("v") = d;
                    auto res = assem.evaluateExpression(contents, b.line);
                    d = number<uint8_t>(res);
                }
                mach.writeByte(d);
            }
        } else if (auto* sv = any_cast<std::string_view>(&data)) {
            auto utext = utils::utf8_decode(*sv);
            for (size_t i = 0; i < utext.size(); i++) {
                auto d = utext[i];
                syms.erase("i");
                syms.set("i", any_num(i));
                for (auto const& b : blocks) {
                    auto contents = strip_space(b.contents);
                    assem.getSymbols().at<Number>("v") = d;
                    auto res = assem.evaluateExpression(contents, b.line);
                    d = number<wchar_t>(res);
                }
                mach.writeByte(char_translate.at(d));
            }
        } else {
            auto size = number<size_t>(data);
            LOGD("Fill %d bytes", size);
            for (size_t i = 0; i < size; i++) {
                syms.erase("i");
                syms.set("i", any_num(i));
                uint8_t d = 0;
                for (auto const& b : blocks) {
                    auto contents = strip_space(b.contents);
                    assem.getSymbols().at<Number>("v") = d;
                    auto res = assem.evaluateExpression(contents, b.line);
                    d = number<uint8_t>(res);
                }
                mach.writeByte(d);
            }
        }
    });

    assem.registerMeta("map", [&](auto const& text, auto const& blocks) {
        std::vector<uint8_t> result;
        auto parts = utils::split(std::string(text), '=');
        if (parts.size() != 2) {
            throw parse_error("!map expected equals sign");
        }
        auto name = utils::lrstrip(parts[0]);
        auto data = assem.evaluateExpression(parts[1]);

        auto* vec = any_cast<std::vector<uint8_t>>(&data);
        size_t size = vec ? vec->size() : number<size_t>(data);

        for (size_t i = 0; i < size; i++) {
            uint8_t d = vec ? (*vec)[i] : 0;
            for (auto const& b : blocks) {
                assem.getSymbols().at<Number>("i") = static_cast<Number>(i);
                assem.getSymbols().at<Number>("v") = d;
                auto res = assem.evaluateExpression(b.contents);
                d = number<uint8_t>(res);
                result.push_back(d);
            }
        }
        assem.getSymbols().set(name, result);
    });

    assem.registerMeta("org", [&](auto const& text, auto const&) {
        auto org = number<int32_t>(assem.evaluateExpression(text));
        mach.getCurrentSection().setPC(org);
    });

    assem.registerMeta("cpu", [&](auto const& text, auto const&) {
        if (text == "6502") {
            mach.setCpu(Machine::CPU_6502);
        } else if (text == "65C02") {
            mach.setCpu(Machine::CPU_65C02);
        } else {
            throw parse_error("Unknown CPU");
        }
    });

    assem.registerMeta("align", [&](auto const& text, auto const&) {
        auto bytes = number<int32_t>(assem.evaluateExpression(text));
        int bits = 0;
        while (bytes > 1) {
            bits++;
            bytes /= 2;
        }
        auto mask = 0xffff >> (16 - bits);
        LOGD("%d PC must align with %x", bits, mask);
        while ((mach.getPC() & mask) != 0) {
            mach.writeByte(0);
        }
    });

    assem.registerMeta("pc", [&](auto const& text, auto const&) {
        auto org = number<int32_t>(assem.evaluateExpression(text));
        mach.getCurrentSection().pc = org;
    });

    assem.registerMeta("ds", [&](auto const& text, auto const&) {
        auto sz = number<int32_t>(assem.evaluateExpression(text));
        mach.getCurrentSection().pc += sz;
    });

    assem.registerMeta(
        "log", [&](auto const& text, auto const&) { assem.addLog(text); });
    assem.registerMeta(
        "check", [&](auto const& text, auto const&) { assem.addCheck(text); });

    assem.registerMeta("print", [&](auto const& text, auto const&) {
        if (!assem.isFinalPass()) return;
        auto args = assem.evaluateList(text);
        for (auto const& arg : args) {
            printArg(arg);
        }
        fmt::print("\n");
    });

    assem.registerMeta("section", [&](auto const& text, auto const& blocks) {
        auto& syms = assem.getSymbols();
        syms.set("NO_STORE", num(0x100000000));
        syms.set("TO_PRG", num(0x200000000));
        syms.set("RO", num(0x400000000));
        auto args = assem.evaluateList(text);

        syms.erase("NO_STORE");
        syms.erase("TO_PRG");
        syms.erase("RO");

        if (args.empty()) {
            throw parse_error("Too few arguments");
        }
        auto sectionArgs = parseArgs(args);

        if (sectionArgs.name.empty()) {
            sectionArgs.name = "__anon" + std::to_string(mach.getPC());
        }

        auto& section = mach.addSection(sectionArgs);

        if (!blocks.empty()) {
            mach.pushSection(section.name);
            auto sz = section.data.size();
            assem.evaluateBlock(blocks[0].contents);
            if (!section.parent.empty()) {
                mach.getSection(section.parent).pc +=
                    static_cast<int32_t>(section.data.size() - sz);
            }
            mach.popSection();
            return;
        }
        mach.setSection(section.name);
    });

    assem.registerMeta("script", [&](auto const& text, auto const&) {
        if (assem.isFirstPass()) {
            auto args = assem.evaluateList(text);
            Check(args.size() == 1, "Incorrect number of arguments");
            auto name = any_cast<std::string_view>(args[0]);
            auto p = utils::path(name);
            if (p.is_relative()) {
                p = assem.getCurrentPath() / p;
            }
            assem.addScript(p);
        }
    });

    assem.registerMeta("include", [&](auto const& text, auto const&) {
        auto args = assem.evaluateList(text);
        Check(args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(args[0]);
        auto p = assem.evaluatePath(name);
        auto source = assem.includeFile(p.string());
        assem.evaluateBlock(source, p.string());
    });

    assem.registerMeta("incbin", [&](auto const& text, auto const&) {
        auto args = assem.evaluateList(text);
        Check(args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(args[0]);
        auto p = utils::path(name);
        if (p.is_relative()) {
            p = assem.getCurrentPath() / p;
        }
        utils::File f{p};
        for (auto const& b : f.readAll()) {
            mach.writeByte(b);
        }
    });

    assem.registerMeta("assem", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");
        auto args = assem.evaluateList(text);
        auto sym = std::any_cast<std::string_view>(args[0]);
        auto start = number<uint32_t>(args[1]);
        Section s{"__gen" + std::to_string(mach.getPC()), start};
        auto& section = mach.addSection(s);
        mach.pushSection(section.name);
        assem.evaluateBlock(blocks[0].contents, blocks[0].line);
        assem.getSymbols().set(sym, section.data);
        mach.popSection();
        mach.removeSection(section.name);
    });

    assem.registerMeta("rept", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");
        std::any data;
        std::string indexVar = "i";
        auto parts = utils::split(std::string(text), ',');
        if (parts.size() == 2) {
            indexVar = utils::lrstrip(parts[0]);
            data = assem.evaluateExpression(parts[1]);
        } else {
            data = assem.evaluateExpression(text);
        }
        auto* vec = any_cast<std::vector<uint8_t>>(&data);
        size_t count = vec ? vec->size() : number<size_t>(data);

        for (size_t i = 0; i < count; i++) {
            assem.getSymbols().erase(indexVar);
            assem.getSymbols().set(indexVar, any_num(i));
            if (vec) {
                assem.getSymbols().erase("v");
                assem.getSymbols().set("v", any_num((*vec)[i]));
            }
            // localLabel = prefix + std::to_string(i);
            assem.evaluateBlock(blocks[0].contents, blocks[0].line);
        }
    });

    assem.registerMeta("if", [&](auto const& text, auto const& blocks) {
        evalIf(assem, any_cast<Number>(assem.evaluateExpression(text)) != 0,
               blocks);
    });

    assem.registerMeta("ifdef", [&](auto const& text, auto const& blocks) {
        auto rc = assem.getSymbols().is_defined(text);
        evalIf(assem, rc, blocks);
    });

    assem.registerMeta("ifndef", [&](auto const& text, auto const& blocks) {
        auto rc = assem.getSymbols().is_defined(text);
        evalIf(assem, !rc, blocks);
    });

    assem.registerMeta("enum", [&](auto const& text, auto const& blocks) {
        Check(!blocks.empty(), "Expected block");
        auto s = assem.evaluateEnum(blocks[0].contents, blocks[0].line);
        if (text.empty()) {
            for (auto const& [name, v] : s) {
                assem.getSymbols().set(name, v);
            }
        } else {
            assem.getSymbols().set<AnyMap>(std::string(text), s);
        }
    });
}
