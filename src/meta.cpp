#include "assembler.h"
#include "defines.h"
#include "machine.h"
#include "petscii.h"

#include <any>
#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/split.h>
#include <coreutils/utf8.h>
#include <fmt/format.h>
#include <vector>

using namespace std::string_literals;

static std::unordered_map<int32_t, uint8_t> char_translate;

static bool globalCond = true;

enum class Translation
{
    Ascii,
    PetsciiUpper,
    PetsciiLower,
    ScreencodeUpper,
    ScreencodeLower
};

static std::array translationNames{"ascii", "petscii_upper", "petscii_lower",
                                   "screencode_upper", "screencode_lower"};
Translation currentTranslation = Translation::Ascii;

static void setTranslation(Translation t)
{
    int32_t (*ptr)(uint8_t) = nullptr;
    currentTranslation = t;
    switch (t) {
    case Translation::Ascii:
        ptr = [](uint8_t i) -> int32_t { return i; };
        break;
    case Translation::PetsciiUpper:
        ptr = &pet2uni_up;
        break;
    case Translation::PetsciiLower:
        ptr = &pet2uni_lo;
        break;
    case Translation::ScreencodeUpper:
        ptr = &sc2uni_up;
        break;
    case Translation::ScreencodeLower:
        ptr = &sc2uni_lo;
        break;
    }
    char_translate.clear();
    for (int i = 0; i < 128; i++) {

        auto u = ptr(i);
        char_translate[u] = i;
    }
}

static uint8_t translate(uint32_t c)
{
    auto it = char_translate.find(c);
    if (it != char_translate.end()) {
        return it->second;
    }
    auto s = utils::utf8_encode({c});
    throw parse_error(
        fmt::format("Char '{}' (0x{:x}) not available in '{}'", s, c,
                    translationNames[static_cast<int>(currentTranslation)]));
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
            } else if (p->first == "NoStore") {
                result.flags |= NoStorage;
            } else if (p->first == "ToFile") {
                result.flags |= WriteToDisk;
            }
        } else {
            if (i == 0) {
                result.name = std::any_cast<std::string_view>(arg);
            } else if (i == 1) {
                result.start = number<uint32_t>(arg);
            } else if (i == 2) {
                auto res = number<uint64_t>(arg);
                if (res >= 0x1'0000'0000) {
                    result.flags |= (res >> 32);
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
    using Meta = Assembler::Meta;
    auto& mach = assem.getMachine();

    setTranslation(Translation::ScreencodeUpper);

    static auto is_space = [](char const c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    static auto strip_space = [](std::string_view sv) -> std::string_view {
        if (sv.empty()) return sv;
        while (is_space(sv.front())) {
            sv.remove_prefix(1);
        }
        if (sv.empty()) return sv;
        while (is_space(sv.back())) {
            sv.remove_suffix(1);
        }
        return sv;
    };

    assem.registerMeta("rept", [&](Meta const& meta) {
        Check(meta.blocks.size() == 1, "Expected block");
        Check(meta.args.size() == 1, "Expected single argument");
        std::any data = meta.args[0];
        std::string indexVar = "i";
        std::vector<uint8_t>* vec = nullptr;
        size_t count = 0;
        if (auto* p = any_cast<std::pair<std::string_view, std::any>>(&data)) {
            indexVar = p->first;
            count = number<size_t>(p->second);
        } else if ((vec = any_cast<std::vector<uint8_t>>(&data))) {
            count = vec->size();
        } else {
            count = number<size_t>(data);
        }
        auto ll = assem.getLastLabel();
        for (size_t i = 0; i < count; i++) {
            assem.getSymbols().erase(indexVar);
            assem.getSymbols().set(indexVar, any_num(i));
            if (vec != nullptr) {
                assem.getSymbols().erase("v");
                assem.getSymbols().set("v", any_num((*vec)[i]));
            }
            assem.setLastLabel("__rept" + std::to_string(mach.getPC()));
            assem.evaluateBlock(meta.blocks[0]);
        }
        assem.setLastLabel(ll);
    });

    assem.registerMeta("test", [&](Meta const& meta) {
        std::string testName;
        uint32_t start = mach.getPC();
        using sixfive::Reg;
        RegState regs;
        for (auto const& v : meta.args) {
            if (auto const* s = any_cast<std::string_view>(&v)) {
                testName = *s;
            } else if (auto const* n = any_cast<Number>(&v)) {
                start = *n;
            } else if (auto const* p =
                           std::any_cast<std::pair<std::string_view, std::any>>(
                               &v)) {
                if (p->first == "A") {
                    regs.regs[0] = number<unsigned>(p->second);
                } else if (p->first == "X") {
                    regs.regs[1] = number<unsigned>(p->second);
                } else if (p->first == "Y") {
                    regs.regs[2] = number<unsigned>(p->second);
                } else {
                    if (auto sym = assem.getSymbols().get_sym(p->first)) {
                        regs.ram[number<uint16_t>(sym->value)] =
                            number<uint8_t>(p->second);
                    }
                }
            }
        }
        assem.addTest(testName, start, regs);
    });

    assem.registerMeta("macro", [&](Meta const& meta) {
        Check(meta.blocks.size() >= 1, "Expected block");

        auto macroName = any_cast<std::string_view>(meta.args[0]);
        auto macroArgs = any_cast<std::vector<std::string_view>>(meta.args[1]);

        assem.defineMacro(macroName, macroArgs, meta.blocks[0]);
    });

    assem.registerMeta(
        "ascii", [&](Meta const&) { setTranslation(Translation::Ascii); });

    assem.registerMeta("encoding", [&](Meta const& meta) {
        auto name = any_cast<std::string_view>(meta.args[0]);

        for (size_t i = 0; i < translationNames.size(); i++) {
            auto const& n = translationNames[i];
            if (n == name) {
                setTranslation((Translation)i);
                return;
            }
        }
        throw parse_error(fmt::format("Unknown encoding '{}'", name));
    });

    assem.registerMeta("chartrans", [&](Meta const& meta) {
        std::u32string text;
        size_t index = 0;
        for (auto const& v : meta.args) {
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

    assem.registerMeta("text", [&](Meta const& meta) {
        for (auto const& v : meta.args) {
            if (auto* s = any_cast<std::string_view>(&v)) {
                auto ws = utils::utf8_decode(*s);
                for (auto c : ws) {
                    // LOGI("UC %x", (int32_t)c);
                    auto b = translate(c);
                    mach.writeChar(b);
                }
            } else {
                throw parse_error("Need text");
            }
        }
        for (auto const& block : meta.blocks) {
            auto contents = std::string(block.contents);
            auto parts = utils::split(contents, "\n");
            bool first = true;
            for (auto part : parts) {
                std::string_view sv = part;
                sv = strip_space(sv);
                auto ws = utils::utf8_decode(sv);
                if (first) {
                    mach.writeChar(' ');
                    first = false;
                }
                for (auto c : ws) {
                    auto b = translate(c);
                    mach.writeChar(b);
                }
            }
        }
    });

    assem.registerMeta("byte", [&](Meta const& meta) {
        for (auto const& v : meta.args) {
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

    assem.registerMeta("byte3", [&](Meta const& meta) {
        for (auto const& v : meta.args) {
            auto b = number<uint32_t>(v);
            mach.writeByte((b >> 16) & 0xff);
            mach.writeByte((b >> 8) & 0xff);
            mach.writeByte(b & 0xff);
        }
    });

    assem.registerMeta("word", [&](Meta const& meta) {
        for (auto const& v : meta.args) {
            auto w = number<int32_t>(v);
            mach.writeByte(w & 0xff);
            mach.writeByte(w >> 8);
        }
    });

    assem.registerMeta("assert", [&](Meta const& meta) {
        if (!assem.isFinalPass()) return;
        auto v = any_cast<Number>(meta.args[0]);
        if (v == 0.0) {
            std::string_view msg = meta.text;
            if (meta.args.size() > 1) {
                msg = any_cast<std::string_view>(meta.args[1]);
            }
            throw assert_error(std::string(msg));
        }
    });

    assem.registerMeta("if", [&](Meta const& meta) {
        for (size_t i = 0; i < meta.blocks.size(); i++) {
            auto cond = i < meta.args.size() ? number(meta.args[i]) : 1.0;
            globalCond = cond != 0;
            if (cond != 0) {
                assem.evaluateBlock(meta.blocks[i]);
                break;
            }
        }
    });

    assem.registerMeta("elseif", [&](Meta const& meta) {
        if (globalCond) return;

        for (size_t i = 0; i < meta.blocks.size(); i++) {
            auto cond = i < meta.args.size() ? number(meta.args[i]) : 1.0;
            globalCond = cond != 0;
            if (cond != 0) {
                assem.evaluateBlock(meta.blocks[i]);
                break;
            }
        }
    });

    assem.registerMeta("else", [&](Meta const& meta) {
        if (globalCond) return;

        for (size_t i = 0; i < meta.blocks.size(); i++) {
            assem.evaluateBlock(meta.blocks[i]);
        }
        globalCond = true;
    });

    assem.registerMeta("org", [&](Meta const& meta) {
        auto org = number<int32_t>(meta.args[0]);
        mach.getCurrentSection().setPC(org);
    });

    assem.registerMeta("cpu", [&](Meta const& meta) {
        auto text = any_cast<std::string_view>(meta.args[0]);
        if (text == "6502") {
            mach.setCpu(Machine::CPU_6502);
        } else if (text == "65C02") {
            mach.setCpu(Machine::CPU_65C02);
        } else {
            throw parse_error("Unknown CPU");
        }
    });

    assem.registerMeta("align", [&](Meta const& meta) {
        auto bytes = number<int32_t>(meta.args[0]);
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

    assem.registerMeta("pc", [&](Meta const& meta) {
        auto org = number<int32_t>(meta.args[0]);
        mach.getCurrentSection().pc = org;
    });

    assem.registerMeta("ds", [&](Meta const& meta) {
        auto sz = number<int32_t>(meta.args[0]);
        mach.getCurrentSection().pc += sz;
    });

    assem.registerMeta("run", [&](Meta const& meta) {
        assem.addRunnable(meta.blocks[0].contents, meta.line);
    });

    assem.registerMeta("rts", [&](Meta const& meta) {
        mach.addIntercept(mach.getPC(), [](uint32_t) { return true; });
    });

    assem.registerMeta("log", [&](Meta const& meta) {
        auto text = any_cast<std::string_view>(meta.args[0]);
        assem.addLog(text, meta.line);
    });

    assem.registerMeta("check", [&](Meta const& meta) {
        Check(!meta.blocks.empty(), "Expected Expression");
        assem.addCheck(meta.blocks[0], meta.line);
    });

    assem.registerMeta("print", [&](Meta const& meta) {
        if (!assem.isFinalPass()) return;
        for (auto const& arg : meta.args) {
            printArg(arg);
        }
        fmt::print("\n");
    });

    assem.registerMeta("section", [&](Meta const& meta) {
        auto& syms = assem.getSymbols();

        if (meta.args.empty()) {
            throw parse_error("Too few arguments");
        }
        auto sectionArgs = parseArgs(meta.args);

        // if (sectionArgs.name.empty()) {
        //    sectionArgs.name = "__anon" + std::to_string(mach.getPC());
        //}

        auto& section = mach.addSection(sectionArgs);

        if (!meta.blocks.empty()) {
            mach.pushSection(section.name);
            auto sz = section.data.size();
            assem.evaluateBlock(meta.blocks[0]);
            if (!section.parent.empty()) {
                mach.getSection(section.parent).pc +=
                    static_cast<int32_t>(section.data.size() - sz);
            }
            syms.set("sections."s + std::string(section.name), section.data);
            mach.popSection();
            return;
        }
        mach.setSection(section.name);
    });

    // count,value
    // count,lambda
    // vector
    // string
    assem.registerMeta("fill", [&](Meta const& meta) {
        auto& syms = assem.getSymbols();
        auto data = meta.args[0];
        if (auto* vec = any_cast<std::vector<uint8_t>>(&data)) {
            for (size_t i = 0; i < vec->size(); i++) {
                uint8_t d = (*vec)[i];
                mach.writeByte(d);
            }
        } else if (auto* nv = any_cast<std::vector<Number>>(&data)) {
            for (size_t i = 0; i < vec->size(); i++) {
                auto d = (*nv)[i];
                if (d > 255 || d < -127) {
                    throw parse_error("Value does not fit");
                }
                mach.writeByte(d);
            }
        } else if (auto* sv = any_cast<std::string_view>(&data)) {
            auto utext = utils::utf8_decode(*sv);
            for (size_t i = 0; i < utext.size(); i++) {
                auto d = utext[i];
                mach.writeByte(translate(d));
            }
        } else {
            auto size = number<size_t>(data);
            uint8_t d = 0;
            Assembler::Macro* macro = nullptr;
            Assembler::Call call;
            if (meta.args.size() > 1) {
                data = meta.args[1];
                if (auto* val = any_cast<Number>(&data)) {
                    d = *val;
                } else {
                    // LOGI("Found macro");
                    macro = any_cast<Assembler::Macro>(&data);
                    for (auto const& i : macro->args) {
                        // LOGI("Arg %s", i);
                    }
                    call.args.resize(macro->args.size());
                }
            }

            // LOGI("Fill %d bytes", size);
            for (size_t i = 0; i < size; i++) {
                if (macro != nullptr) {
                    if (call.args.size() > 0) {
                        call.args[0] = any_num(i);
                    }
                    auto res = assem.applyDefine(*macro, call);
                    d = number<uint8_t>(res);
                }
                mach.writeByte(d);
            }
        }
    });

    assem.registerMeta("include", [&](Meta const& meta) {
        Check(meta.args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(meta.args[0]);
        auto p = assem.evaluatePath(name);
        auto fileName = persist(p.string());
        auto block = assem.includeFile(fileName);
        assem.evaluateBlock(block);
    });

    assem.registerMeta("script", [&](Meta const& meta) {
        if (assem.isFirstPass()) {
            Check(meta.args.size() == 1, "Incorrect number of arguments");
            auto name = any_cast<std::string_view>(meta.args[0]);
            auto p = fs::path(name);
            if (p.is_relative()) {
                p = assem.getCurrentPath() / p;
            }
            assem.addScript(p);
        }
    });

    assem.registerMeta("incbin", [&](Meta const& meta) {
        Check(meta.args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(meta.args[0]);
        auto p = fs::path(name);
        if (p.is_relative()) {
            p = assem.getCurrentPath() / p;
        }
        utils::File f{p.string()};
        for (auto const& b : f.readAll()) {
            mach.writeByte(b);
        }
    });

    assem.registerMeta("enum", [&](Meta const& meta) {
        Check(!meta.blocks.empty(), "Expected block");
        assem.pushScope(any_cast<std::string_view>(meta.args[0]));
        assem.evaluateBlock(meta.blocks[0]);
        assem.popScope();
    });
}
