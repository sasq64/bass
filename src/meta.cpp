#include "assembler.h"
#include "defines.h"
#include "machine.h"

#include <any>
#include <coreutils/log.h>
#include <coreutils/split.h>
#include <coreutils/text.h>
#include <fmt/format.h>
#include <iostream>
#include <vector>

using namespace std::string_literals;

static uint8_t translate(uint8_t c)
{

    if (c >= 'a' && c <= 'z') return c - 'a' + 1;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 0x41;
    return c;
}

static void evalIf(Assembler& a, bool cond,
                   std::vector<std::string_view> const& blocks)
{
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

    a.registerMeta("macro", [&](auto const& text, auto const& blocks) {
        auto def = a.evaluateDefinition(text);
        a.defineMacro(def.name, def.args, blocks[0]);
    });

    a.registerMeta("define", [&](auto const& text, auto const& blocks) {
      auto def = a.evaluateDefinition(text);
      a.addDefine(def.name, def.args, blocks[0]);
    });

    a.registerMeta("text", [&](auto const& text, auto const&) {
        auto list = a.evaluateList(text);
        for (auto const& v : list) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                for (auto c : *s) {
                    c = translate(c);
                    mach.writeByte(c);
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
                    mach.writeByte(c);
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
        if (v == 0) {
            std::string_view msg = text;
            if (args.size() > 1) {
                msg = any_cast<std::string_view>(args[1]);
            }
            throw assert_error(std::string(msg));
        }
    });

    a.registerMeta("fill", [&](auto const& text, auto const& blocks) {
        std::any data = a.evaluateExpression(text);
        auto* vec = any_cast<std::vector<uint8_t>>(&data);
        size_t size = vec ? vec->size() : number<size_t>(data);

        for (size_t i = 0; i < size; i++) {
            uint8_t d = vec ? (*vec)[i] : 0;
            for (auto const& b : blocks) {
                a.getSymbols()["i"] = (Number)i;
                a.getSymbols()["v"] = (Number)d;
                auto res = a.evaluateExpression(b);
                d = number<uint8_t>(res);
            }
            mach.writeByte(d);
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
                a.getSymbols()["i"] = (Number)i;
                a.getSymbols()["v"] = (Number)d;
                auto res = a.evaluateExpression(b);
                d = number<uint8_t>(res);
                result.push_back(d);
            }
        }
        a.getSymbols()[name] = result;
    });

    // TODO: Redundant, use fill
    a.registerMeta("block", [&](auto const& text, auto const&) {
        std::any data = a.evaluateExpression(text);
        auto const& vec = any_cast<std::vector<uint8_t>>(data);
        for (auto d : vec) {
            mach.writeByte(d);
        }
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
        auto pc = mach.getPC();
        auto sz = number<int32_t>(a.evaluateExpression(text));
        mach.getCurrentSection().pc += sz;
    });

    a.registerMeta("print", [&](auto const& text, auto const&) {
        if (!a.isFinalPass()) return;
        auto args = a.evaluateList(text);
        for (auto const& arg : args) {
            if (auto* l = any_cast<Number>(&arg)) {
                std::cout << *l;
            } else if (auto* s = any_cast<std::string_view>(&arg)) {
                std::cout << *s;
            } else if (auto* v = any_cast<std::vector<uint8_t>>(&arg)) {
                for (auto const& item : *v) {
                    fmt::print("{:02x} ", item);
                }
            }
        }
        std::cout << "\n";
    });

    a.registerMeta("section", [&](auto const& text, auto const&) {
        auto args = a.evaluateList(text);
        if (args.size() == 0) {
            throw parse_error("Too few arguments");
        }
        auto name = any_cast<std::string_view>(args[0]);
        uint16_t start = 0;
        if (args.size() == 1) {
            mach.setSection(std::string(name));
            return;
        }

        if (args.size() > 1) {
            start = number<uint16_t>(args[1]);
        }
        // LOGI("Section %s at %x", name, start);
        uint16_t pc = start;
        int32_t flags = 0;
        if (args.size() > 2) {
            a.getSymbols()["NO_STORE"] = (Number)0x100000000;
            a.getSymbols()["TO_PRG"] = (Number)0x200000000;
            a.getSymbols()["RO"] = (Number)0x400000000;
            auto res = number<uint64_t>(args[2]);
            if (res >= 0x100000000) {
                flags = res >> 32;
            } else {
                pc = res;
            }
        }
        auto& s = mach.addSection(std::string(name), start);
        s.pc = pc;
        s.flags = flags;
    });

    a.registerMeta("include", [&](auto const& text, auto const&) {
        auto args = a.evaluateList(text);
        auto name = any_cast<std::string_view>(args[0]);
        auto source = a.includeFile(name);
        a.evaluateBlock(source, name);
    });

    a.registerMeta("rept", [&](auto const& text, auto const& blocks) {
        if (blocks.empty()) {
            throw parse_error("No block for !rept");
        }

        std::any data = a.evaluateExpression(text);
        auto* vec = any_cast<std::vector<uint8_t>>(&data);
        size_t count = vec ? vec->size() : number<size_t>(data);

        for (Number i = 0; i < count; i++) {
            a.getSymbols()["i"] = i;
            if (vec) {
                a.getSymbols()["v"] = (*vec)[i];
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
        evalIf(a, a.getSymbols().get(std::string(text)).has_value(), blocks);
    });

    a.registerMeta("ifndef", [&](auto const& text, auto const& blocks) {
        evalIf(a, !a.getSymbols().get(std::string(text)).has_value(), blocks);
    });

    a.registerMeta("enum", [&](auto const& text, auto const& blocks) {
        auto s = a.evaluateEnum(blocks[0]);
        if (text.empty()) {
            s.forAll([&](auto const& name, auto const& v) {
                a.getSymbols()[name] = v;
            });
        } else {
            a.getSymbols()[std::string(text)] = s;
        }
    });
}
