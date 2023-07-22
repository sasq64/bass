#include "assembler.h"
#include "chars.h"
#include "defines.h"
#include "machine.h"

#include <lib.h>
#include <shrink_inmem.h>

#include <any>
#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/split.h>
#include <coreutils/utf8.h>
#include <fmt/format.h>
#include <vector>

using namespace std::string_literals;

namespace {

SectionInitializer parseArgs(std::vector<std::any> const& args)
{
    SectionInitializer result;
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
            } else if (p->first == "Compress") {
                result.flags |= Compressed;
            } else if (p->first == "Backwards") {
                result.flags |= Backwards;
            }
        } else {
            if (i == 0) {
                result.name = std::any_cast<std::string_view>(arg);
            } else if (i == 1) {
                result.start = number<int32_t>(arg);
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

} // namespace

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

void metaText(Assembler::Meta const& meta, Machine& mach)
{
    for (auto const& v : meta.args) {
        if (auto const* s = std::any_cast<std::string_view>(&v)) {
            auto ws = utils::utf8_decode(*s);
            for (auto c : ws) {
                auto b = translateChar(c);
                mach.writeChar(b);
            }
        } else if (auto const* n = std::any_cast<Number>(&v)) {
            mach.writeByte(*n);
        } else {
            throw parse_error("Need text");
        }
    }
    for (auto const& block : meta.blocks) {
        auto contents = std::string(block.contents);
        auto parts = utils::split(contents, "\n");
        bool first = true;
        for (auto const* part : parts) {
            std::string_view sv = part;
            sv = strip_space(sv);
            auto ws = utils::utf8_decode(sv);
            if (first) {
                mach.writeChar(' ');
                first = false;
            }
            for (auto c : ws) {
                auto b = translateChar(c);
                mach.writeChar(b);
            }
        }
    }
}

extern Translation currentTranslation;

void initMeta(Assembler& assem)
{
    using std::any_cast;
    using Meta = Assembler::Meta;
    static bool globalCond = true;
    auto& mach = assem.getMachine();

    setTranslation(Translation::ScreencodeUpper);

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
        assem.getSymbols().erase(indexVar);
        for (size_t i = 0; i < count; i++) {
            assem.getSymbols().set(indexVar, any_num(i));
            if (vec != nullptr) {
                assem.getSymbols().erase("v");
                assem.getSymbols().set("v", any_num((*vec)[i]));
            }
            //assem.setLastLabel("__rept" + std::to_string(mach.getPC()));
            assem.evaluateBlock(meta.blocks[0]);
            assem.getSymbols().erase(indexVar);
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
                start = static_cast<uint32_t>(*n);
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
        Check(!meta.blocks.empty(), "Expected block");

        auto macroName = any_cast<std::string_view>(meta.args[0]);
        auto macroArgs = any_cast<std::vector<std::string_view>>(meta.args[1]);

        assem.defineMacro(macroName, macroArgs, meta.blocks[0]);
    });

    assem.registerMeta(
        "ascii", [&](Meta const&) { setTranslation(Translation::Ascii); });

    assem.registerMeta("encoding", [&](Meta const& meta) {
        auto name = any_cast<std::string_view>(meta.args[0]);

        setTranslation(name);
    });

    assem.registerMeta("chartrans", [&](Meta const& meta) {
        std::u32string text;
        size_t index = 0;
        for (auto const& v : meta.args) {
            if (auto const* s = any_cast<std::string_view>(&v)) {
                text = utils::utf8_decode(*s);
                index = 0;
            } else {
                Check(!text.empty(),
                      "First argument must be a (non-empty) string");
                Check(index < text.length(), "Arguments exceed string length");
                auto n = number<uint8_t>(v);
                auto c = text[index++];
                LOGD("%x %x", n, (uint16_t)c);
                setTranslation(c, n);
            }
        }
    });

    // ACME compability
    assem.registerMeta("scr", [&](Meta const& meta) {
        auto ct = currentTranslation;
        setTranslation(Translation::ScreencodeLower);
        metaText(meta, mach);
        setTranslation(ct);
    });

    assem.registerMeta("pet", [&](Meta const& meta) {
      auto ct = currentTranslation;
      setTranslation(Translation::PetsciiLower);
      metaText(meta, mach);
      setTranslation(ct);
    });

    assem.registerMeta("text", [&](Meta const& meta) {
        metaText(meta, mach);
    });

    assem.registerMeta("byte", [&](Meta const& meta) {
        for (auto const& v : meta.args) {
            if (auto const* s = any_cast<std::string_view>(&v)) {
                for (auto c : *s) {
                    mach.writeChar(c);
                }
            } else {
                auto b = number<int8_t>(v);
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

        for (auto const& block : meta.blocks) {
            assem.evaluateBlock(block);
        }
        globalCond = true;
    });

    assem.registerMeta("org", [&](Meta const& meta) {
        auto org = number<uint32_t>(meta.args[0]);
        auto& section = mach.addSection({"", org});
        mach.setSection(section.name);
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
        if (meta.args.empty()) {
            mach.getCurrentSection().pc ++;
            return;
        }
        auto sz = number<int32_t>(meta.args[0]);
        mach.getCurrentSection().pc += sz;
    });

    assem.registerMeta("run", [&](Meta const& meta) {
        assem.addRunnable(meta.blocks[0].contents, meta.line);
    });

    assem.registerMeta("rts", [&](Meta const&) {
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

    assem.registerMeta("trace", [&](Meta const& meta) {
        for (auto const& arg : meta.args) {
            printArg(arg);
        }
        fmt::print("\n");
    });

    assem.registerMeta("section", [&](Meta const& meta) {
        if (meta.args.empty()) {
            throw parse_error("Too few arguments");
        }
        auto sectionArgs = parseArgs(meta.args);

        // if (sectionArgs.name.empty()) {
        //    sectionArgs.name = "__anon" + std::to_string(mach.getPC());
        //}

        auto& section = mach.addSection(sectionArgs);

        if (!meta.blocks.empty()) {
            auto& syms = assem.getSymbols();
            mach.pushSection(section.name);
            auto sz = section.data.size();
            auto pc = section.pc; 
            assem.evaluateBlock(meta.blocks[0]);

            if (!section.parent.empty()) {
                mach.getSection(section.parent).pc +=
                    static_cast<int32_t>(section.get_size() - sz);
            }

            if ((section.flags & Backwards) != 0) {
                section.flags |= Compressed;
            }


            auto p = "sections."s + std::string(section.name);
            if ((section.flags & Compressed) != 0) {
                LOGI("Packing %s", p.c_str());
#ifdef USE_BASS_VALUEPROVIDER
                section.storeOriginalSize();
#endif
                std::vector<uint8_t> packed(0x10000);
                int flags = LZSA_FLAG_RAW_BLOCK;
                if ((section.flags & Backwards) != 0) {
                    flags |= LZSA_FLAG_RAW_BACKWARD;
                }
                auto packed_size =
                    lzsa_compress_inmem(section.data.data(), packed.data(),
                            section.data.size(), packed.size(), flags, 0, 1);
                packed.resize(packed_size);
                syms.set(p + ".original_size", section.data.size());
                section.data = packed;
            }
            syms.set(p + ".data", section.data);
            syms.set(p + ".start", section.start);
            syms.set(p + ".pc", pc);
            syms.set(p + ".size", section.data.size());
#ifdef USE_BASS_VALUEPROVIDER
            section.storeSymbols();
#endif
            mach.popSection();
            return;
        }
        mach.setSection(section.name);
    });

    assem.registerMeta("fill", [&](Meta const& meta) {
        ::Check(!meta.args.empty(), "Invalid !fill meta command");
        auto data = meta.args[0];
        size_t size = 0;
        bool firstConst = false;

        // Create source lambda depending on first argument
        std::function<Number(size_t)> src;
        if (auto* vec = any_cast<std::vector<uint8_t>>(&data)) {
            size = vec->size();
            src = [v = *vec](size_t i) -> Number { return v[i]; };
        } else if (auto* nv = any_cast<std::vector<Number>>(&data)) {
            size = nv->size();
            src = [v = *nv](size_t i) -> Number {
                auto d = v[i];
                return d;
            };
        } else if (auto* sv = any_cast<std::string_view>(&data)) {
            LOGI("Fill string %s", *sv);
            auto utext = utils::utf8_decode(*sv);
            size = utext.size();
            src = [utext](size_t i) -> Number {
                return translateChar(utext[i]);
            };
        } else {
            size = number<size_t>(data);
            src = [](size_t i) { return static_cast<Number>(i); };
            firstConst = true;
        }

        std::function<uint8_t(size_t, Number)> tx;
        Assembler::Macro* macro = nullptr;
        Assembler::Call call;
        // Create transform lambda depending on second argument
        if (meta.args.size() == 1) {
            // If first argument was a constant, fill with zeroes
            if (firstConst) {
                tx = [](size_t, Number) -> uint8_t { return 0; };
            } else {
                tx = [](size_t, Number n) { return static_cast<uint8_t>(n); };
            }
        } else {
            data = meta.args[1];
            if (auto* val = any_cast<Number>(&data)) {
                auto n = static_cast<uint8_t>(*val);
                tx = [n](size_t, Number) -> uint8_t { return n; };
            } else {
                macro = any_cast<Assembler::Macro>(&data);
                ::Check(macro != nullptr, "Invalid !fill macro");
                call.args.resize(macro->args.size());
                tx = [&](size_t i, Number n) -> uint8_t {
                    if (call.args.size() >= 1) {
                        call.args[0] = any_num(n);
                    }
                    if (call.args.size() >= 2) {
                        call.args[1] = any_num(i);
                    }
                    auto res = assem.applyDefine(*macro, call);
                    return number<uint8_t>(res);
                };
            }
        }

        for (size_t i = 0; i < size; i++) {
            auto n = src(i);
            n = tx(i, src(i));
            if (n > 255 || n < -127) {
                throw parse_error("Value does not fit");
            }
            mach.writeByte(n);
        }
    });

    assem.registerMeta("include", [&](Meta const& meta) {
        Check(meta.args.size() == 1, "Incorrect number of arguments");
        auto name = any_cast<std::string_view>(meta.args[0]);
        auto fullPath = assem.evaluatePath(name);
        auto fileName = persist(fullPath.string());
        auto block = assem.includeFile(fileName);
        auto saved = assem.getCurrentPath();
        assem.setCurrentPath(fullPath.parent_path());
        assem.evaluateBlock(block);
        assem.setCurrentPath(saved);
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
        utils::File const f{p.string()};
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
