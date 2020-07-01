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
                   std::vector<std::string_view> const& blocks)
{
    if (blocks.empty()) {
        throw parse_error("Expected block");
    }
    if (cond) {
        a.evaluateBlock(blocks[0]);
    } else {
        if (blocks.size() == 3) {
            if (std::any_cast<std::string_view>(blocks[1]) != "else") {
                throw parse_error("Expected 'else'");
            }
            a.evaluateBlock(blocks[2]);
        }
    }
}

void initMeta(Assembler& a)
{
    using std::any_cast;
    auto& mach = a.getMachine();

    resetTranslate();

    a.registerMeta("test", [&](auto const& text, auto const& blocks) {
        auto args = a.evaluateList(text);
        std::string testName;

        for (auto const& v : args) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                testName = *s;
            } else if (auto* s = any_cast<Number>(&v)) {
                a.testLocation = *s;
            }
        }

        if (!testName.empty()) {
        Check(blocks.size() == 1, "Expected block");
            auto contents = blocks[0];
            auto res = a.runTest(testName, contents);
            a.getSymbols().set("tests."s + testName, res, 0);
        }
    });

    a.registerMeta("macro", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");
        auto def = a.evaluateDefinition(text);
        a.defineMacro(def.name, def.args, blocks[0]);
    });

    a.registerMeta("define", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");
        auto def = a.evaluateDefinition(text);
        a.addDefine(def.name, def.args, blocks[0]);
    });

    a.registerMeta("chartrans", [&](auto const& args, auto const&) {
        if (args.empty()) {
            resetTranslate();
            return;
        }
        auto list = a.evaluateList(args);
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
                LOGI("%x %x", n, (uint16_t)c);
                char_translate[c] = n;
            }
        }
    });

    a.registerMeta("text", [&](auto const& text, auto const&) {
        auto list = a.evaluateList(text);
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

    a.registerMeta("byte", [&](auto const& text, auto const&) {
        auto list = a.evaluateList(text);
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

    a.registerMeta("byte3", [&](auto const& text, auto const&) {
        auto list = a.evaluateList(text);
        for (auto const& v : list) {
            auto b = number<uint32_t>(v);
            mach.writeByte((b >> 16) & 0xff);
            mach.writeByte((b >> 8) & 0xff);
            mach.writeByte(b & 0xff);
        }
    });

    a.registerMeta("word", [&](auto const& text, auto const&) {
        auto list = a.evaluateList(text);
        for (auto const& v : list) {
            auto w = number<int32_t>(v);
            mach.writeByte(w & 0xff);
            mach.writeByte(w >> 8);
        }
    });

    a.registerMeta("assert", [&](auto const& text, auto const&) {
        if (!a.isFinalPass()) return;
        auto args = a.evaluateList(text);
        auto v = any_cast<Number>(args[0]);
        if (v == 0.0) {
            std::string_view msg = text;
            if (args.size() > 1) {
                msg = any_cast<std::string_view>(args[1]);
            }
            throw assert_error(std::string(msg));
        }
    });

    a.registerMeta("fill", [&](auto const& text, auto const& blocks) {
        std::any data = a.evaluateExpression(text);

        auto& syms = a.getSymbols();

        if (auto* vec = any_cast<std::vector<uint8_t>>(&data)) {
            for (size_t i = 0; i < vec->size(); i++) {
                uint8_t d = (*vec)[i];
                syms.erase("i");
                syms.set("i", i);
                for (auto const& b : blocks) {
                    a.getSymbols().at<Number>("v") = d;
                    auto res = a.evaluateExpression(b);
                    d = number<uint8_t>(res);
                }
                mach.writeByte(d);
            }
        } else if (auto* sv = any_cast<std::string_view>(&data)) {
            auto text = utils::utf8_decode(*sv);
            for (size_t i = 0; i < text.size(); i++) {
                auto d = text[i];
                syms.erase("i");
                syms.set("i", i);
                for (auto const& b : blocks) {
                    a.getSymbols().at<Number>("v") = d;
                    auto res = a.evaluateExpression(b);
                    d = number<wchar_t>(res);
                }
                mach.writeByte(char_translate.at(d));
            }
        } else {
            auto size = number<size_t>(data);
            for (size_t i = 0; i < size; i++) {
                syms.erase("i");
                syms.set("i", i);
                uint8_t d = 0;
                for (auto const& b : blocks) {
                    a.getSymbols().at<Number>("v") = d;
                    auto res = a.evaluateExpression(b);
                    d = number<uint8_t>(res);
                }
                mach.writeByte(d);
            }
        }
    });

    a.registerMeta("map", [&](auto const& text, auto const& blocks) {
        std::vector<uint8_t> result;
        auto parts = utils::split(std::string(text), '=');
        if (parts.size() != 2) {
            throw parse_error("!map expected equals sign");
        }
        auto name = utils::lrstrip(parts[0]);
        auto data = a.evaluateExpression(parts[1]);

        auto* vec = any_cast<std::vector<uint8_t>>(&data);
        size_t size = vec ? vec->size() : number<size_t>(data);

        for (size_t i = 0; i < size; i++) {
            uint8_t d = vec ? (*vec)[i] : 0;
            for (auto const& b : blocks) {
                a.getSymbols().at<Number>("i") = static_cast<Number>(i);
                a.getSymbols().at<Number>("v") = d;
                auto res = a.evaluateExpression(b);
                d = number<uint8_t>(res);
                result.push_back(d);
            }
        }
        a.getSymbols().set(name, result);
    });

    a.registerMeta("org", [&](auto const& text, auto const&) {
        auto org = number<int32_t>(a.evaluateExpression(text));
        mach.addSection("", org);
    });

    a.registerMeta("align", [&](auto const& text, auto const&) {
        auto bytes = number<int32_t>(a.evaluateExpression(text));
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

    a.registerMeta("pc", [&](auto const& text, auto const&) {
        auto org = number<int32_t>(a.evaluateExpression(text));
        mach.getCurrentSection().pc = org;
    });

    a.registerMeta("ds", [&](auto const& text, auto const&) {
        auto sz = number<int32_t>(a.evaluateExpression(text));
        mach.getCurrentSection().pc += sz;
    });

    a.registerMeta("print", [&](auto const& text, auto const&) {
        if (!a.isFinalPass()) return;
        auto args = a.evaluateList(text);
        for (auto const& arg : args) {
            if (auto* l = any_cast<Number>(&arg)) {
                fmt::print("{}", *l);
            } else if (auto* s = any_cast<std::string_view>(&arg)) {
                fmt::print("{}", *s);
            } else if (auto* v = any_cast<std::vector<uint8_t>>(&arg)) {
                for (auto const& item : *v) {
                    fmt::print("{:02x} ", item);
                }
            }
        }
        fmt::print("\n");
    });

    a.registerMeta("section", [&](auto const& text, auto const& blocks) {
        auto& syms = a.getSymbols();
        syms.set("NO_STORE", num(0x100000000));
        syms.set("TO_PRG", num(0x200000000));
        syms.set("RO", num(0x400000000));
        auto args = a.evaluateList(text);

        int32_t offset = -1;
        int32_t start = -1;
        std::string name;
        int32_t pc = -1;
        int32_t flags = 0;
        int i = 0;

        if (args.empty()) {
            throw parse_error("Too few arguments");
        }
        for (auto const& a : args) {
            if (auto* p = any_cast<std::pair<std::string_view, std::any>>(&a)) {
                if (p->first == "start") {
                    start = number<int32_t>(p->second);
                    pc = start;
                } else if (p->first == "offset") {
                    offset = number<int32_t>(p->second);
                }
            } else {
                if (i == 0) {
                    name = any_cast<std::string_view>(a);
                } else if (i == 1) {
                    start = number<uint32_t>(a);
                    pc = start;
                } else if (i == 2) {
                    auto res = number<uint64_t>(a);
                    if (res >= 0x100000000) {
                        flags = res >> 32;
                    } else {
                        pc = static_cast<uint32_t>(res);
                    }
                }
                i++;
            }
        }

        auto lastSection = mach.getCurrentSection().name;

        if (start == -1) {
            mach.setSection(std::string(name));
            if (!blocks.empty()) {
                a.evaluateBlock(blocks[0]);
                mach.setSection(lastSection);
            }
            return;
        }

        auto& s = mach.addSection(std::string(name), start);
        s.pc = pc;
        s.flags = flags;
    });

    a.registerMeta("script", [&](auto const& text, auto const&) {
        if (a.isFirstPass()) {
            auto args = a.evaluateList(text);
            Check(args.size() == 1, "Incorrect number of arguments");
            auto name = any_cast<std::string_view>(args[0]);
            auto p = utils::path(name);
            if (p.is_relative()) {
                p = a.getCurrentPath() / p;
            }
            a.addScript(p);
        }
    });

    a.registerMeta("include", [&](auto const& text, auto const&) {
        auto args = a.evaluateList(text);
        Check(args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(args[0]);
        auto source = a.includeFile(name);
        a.evaluateBlock(source, name);
    });

    a.registerMeta("incbin", [&](auto const& text, auto const&) {
        auto args = a.evaluateList(text);
        Check(args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(args[0]);
        auto p = utils::path(name);
        if (p.is_relative()) {
            p = a.getCurrentPath() / p;
        }
        utils::File f{p};
        for (auto const& b : f.readAll()) {
            mach.writeByte(b);
        }
    });

    a.registerMeta("rept", [&](auto const& text, auto const& blocks) {
        Check(blocks.size() == 1, "Expected block");

        std::any data;
        std::string indexVar = "i";
        auto parts = utils::split(std::string(text), ',');
        if (parts.size() == 2) {
            indexVar = utils::lrstrip(parts[0]);
            data = a.evaluateExpression(parts[1]);
        } else {
            data = a.evaluateExpression(text);
        }
        auto* vec = any_cast<std::vector<uint8_t>>(&data);
        size_t count = vec ? vec->size() : number<size_t>(data);

        for (size_t i = 0; i < count; i++) {
            a.getSymbols().erase(indexVar);
            a.getSymbols().set(indexVar, any_num(i));
            if (vec) {
                a.getSymbols().erase("v");
                a.getSymbols().set("v", any_num((*vec)[i]));
            }
            a.evaluateBlock(blocks[0]);
        }
    });

    a.registerMeta("if", [&](auto const& text,
                             std::vector<std::string_view> const& blocks) {
        evalIf(a, any_cast<Number>(a.evaluateExpression(text)) != 0, blocks);
    });

    a.registerMeta("ifdef", [&](auto const& text,
                                std::vector<std::string_view> const& blocks) {
        auto rc = a.getSymbols().is_defined(text);
        evalIf(a, rc, blocks);
    });

    a.registerMeta("ifndef", [&](auto const& text, auto const& blocks) {
        auto rc = a.getSymbols().is_defined(text);
        evalIf(a, !rc, blocks);
    });

    a.registerMeta("enum", [&](auto const& text, auto const& blocks) {
        Check(!blocks.empty(), "Expected block");
        auto s = a.evaluateEnum(blocks[0]);
        if (text.empty()) {
            for (auto const& [name, v] : s) {
                a.getSymbols().set(name, v);
            }
        } else {
            a.getSymbols().set<AnyMap>(std::string(text), s);
        }
    });
}
