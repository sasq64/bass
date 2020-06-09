
#include <coreutils/algorithm.h>
#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/path.h>
#include <coreutils/split.h>
#include <coreutils/text.h>

#include "assembler.h"
#include "defines.h"
#include "machine.h"
#include "wrap.h"

#include <fmt/format.h>
#include <string_view>
#include <unordered_set>

extern const char* grammar6502;

using sixfive::AdressingMode;

std::string to_string(std::any const& val)
{
    if (auto const* n = std::any_cast<Number>(&val)) {
        if (*n == static_cast<int64_t>(*n)) return fmt::format("${:x}", *n);
        return fmt::format("{}", *n);
    }
    if (auto const* v = std::any_cast<std::vector<uint8_t>>(&val)) {
        std::string res = "[ ";
        int i = 0;
        for (auto const& b : *v) {
            res += fmt::format("{:02x} ", b);
            if (i++ > 16) {
                res += "... ";
                break;
            }
        }
        return res + "]";
    }
    if (auto const* s = std::any_cast<std::string_view>(&val)) {
        return "\""s + std::string(*s) + "\"";
    }
    if (auto const* s = std::any_cast<std::string>(&val)) {
        return "\""s + *s + "\"";
    }
    return val.type().name();
}

void Assembler::trace(SVWrap const& sv) const
{
    if (!doTrace) return;
    fmt::print("{} : '{}'\n", sv.name(), sv.token());
    for (int i = 0; i < sv.size(); i++) {
        std::any v = sv[i];
        fmt::print("  {}: {}\n", i, to_string(v));
    }
}

void Assembler::debugflags(uint32_t flags)
{
    doTrace = (flags & DEB_TRACE) != 0;
    passDebug = (flags & DEB_PASS) != 0;
}

std::string_view Assembler::includeFile(std::string_view name)
{
    auto fn = std::string(name);
    auto p = utils::path(fn);
    if (p.is_relative()) {
        p = currentPath / p;
        fn = p.string();
    }

    auto it = includes.find(fn);
    if (it != includes.end()) {
        return it->second;
    }

    utils::File f{fn};
    auto source = f.readAllString();
    includes[fn] = source;
    return includes.at(fn);
}

std::any Assembler::evaluateExpression(std::string_view expr)
{
    auto s = ":x:"s + std::string(expr);
    if (!parser.parse(s)) {
        LOGW("Parsing failed");
    }
    return parseResult;
}

Assembler::Def Assembler::evaluateDefinition(std::string_view expr)
{
    auto s = ":d:"s + std::string(expr);
    auto sv = std::string_view(s);
    persist(sv);
    if (!parser.parse(sv)) {
        throw parse_error("Not a definition");
    }
    return std::any_cast<Def>(parseResult);
}

std::vector<std::any> Assembler::evaluateList(std::string_view expr)
{
    auto s = ":a:"s + std::string(expr);
    auto sv = std::string_view(s);
    persist(sv);
    if (!parser.parse(sv)) {
        throw parse_error("Not a list");
    }

    return std::any_cast<std::vector<std::any>>(parseResult);
}

Symbols Assembler::evaluateEnum(std::string_view expr)
{
    auto s = ":e:"s + std::string(expr);
    auto sv = std::string_view(s);
    persist(sv);
    if (!parser.parse(sv)) {
        throw parse_error("Not an enum");
    }

    /* std::any_cast<Symbols>(parseResult) */
    /*     .forAll([](std::string const& name, std::any const& val) { */
    /*         if (!utils::startsWith(name, "__")) */
    /*             fmt::print("{} == {}\n", name, to_string(val)); */
    /*     }); */

    return std::any_cast<Symbols>(parseResult);
}

void Assembler::evaluateBlock(std::string_view block, std::string_view fn)
{
    if (!parser.parse(block, fn.empty() ? fileName.c_str()
                                        : std::string(fn).c_str())) {
        throw parse_error("Syntax error");
    }
}

int Assembler::checkUndefined()
{
    if (undefined.empty()) {
        return DONE;
    }

    auto it = undefined.begin();
    while (it != undefined.end()) {
        // auto full = utils::join(it->parts.begin(), it->parts.end(), ".");
        std::vector<std::string> parts = utils::split(it->name, ".");
        bool erased = false;
        if (passDebug)
            fmt::print("Symbol '{}' was (re)defined after use\n", it->name);
        if (syms.get(parts).has_value()) {
            it = undefined.erase(it);
            erased = true;
        }
        if (it != undefined.end() && syms.get(it->name).has_value()) {
            it = undefined.erase(it);
            erased = true;
        }
        if (!erased) {
            it++;
        }
    }
    if (undefined.empty()) {
        return PASS;
    }
    return ERROR;
}

Symbols Assembler::runTest(std::string_view name, std::string_view contents)
{
    Symbols res;
    if (!finalPass) {
        res["A"] = any_num(0);
        res["X"] = any_num(0);
        res["Y"] = any_num(0);
        res["SR"] = any_num(0);
        res["SP"] = any_num(0);
        res["PC"] = any_num(0);
        res["cycles"] = any_num(0);
        res["ram"] = mach->getRam();
        return res;
    }
    // setTarget(true);
    // mach->startTest();
    auto saved = save();
    auto machSaved = mach->saveState();
    auto pc = mach->getPC();
    LOGI("Testing %s:%s at pc %x", name, contents, pc);
    auto section = mach->getCurrentSection();
    while (true) {
        undefined.clear();
        mach->getCurrentSection() = section;
        lastLabel = "__test_" + std::to_string(pc);
        if (!parser.parse(contents, fileName.c_str())) {
            throw parse_error("Syntax error in test block");
        }
        LOGI("Parsing done");
        auto result = checkUndefined();
        if (result == DONE) break;
        if (result == PASS) continue;
        for (auto const& u : undefined) {
            throw parse_error(fmt::format("'{}' undefined in test", u.name));
        }
    }

    mach->assemble({"rts", AdressingMode::NONE, 0});

    auto cycles = mach->run(pc);
    fmt::print("Test {} : {} cycles\n", name, cycles);

    res["ram"] = mach->getRam();
    auto regs = mach->getRegs();

    res["A"] = any_num(std::get<0>(regs));
    res["X"] = any_num(std::get<1>(regs));
    res["Y"] = any_num(std::get<2>(regs));
    res["SR"] = any_num(std::get<3>(regs));
    res["SP"] = any_num(std::get<4>(regs));
    res["PC"] = any_num(std::get<5>(regs));
    res["cycles"] = any_num(cycles);

    mach->restoreState(machSaved);
    restore(saved);

    return res;
}
std::any Assembler::applyDefine(Macro const& fn, Call const& call)
{
    Symbols shadowed;
    std::vector<std::string> args;
    for (auto const& a : fn.args) {
        args.emplace_back(a);
    }

    for (unsigned i = 0; i < call.args.size(); i++) {
        auto const& v = syms.get(args[i]);
        if (v.has_value()) {
            parser.errors.push_back(
                {0, 0,
                 fmt::format("Function '{}' shadows global symbol {}",
                             call.name, fn.args[i]),
                 ErrLevel::Warning});
            shadowed[args[i]] = v;
            syms.erase(args[i]);
        }
        syms[args[i]] = call.args[i];
        LOGD("%s = %x", fn.args[i],
             (int32_t)std::any_cast<Number>(call.args[i]));
    }

    auto res = evaluateExpression(fn.contents);

    for (unsigned i = 0; i < call.args.size(); i++) {
        syms.erase(args[i]);
    }
    for (auto const& shadow : shadowed) {
        syms[shadow.first] = shadow.second;
    }
    return res;
}

void Assembler::applyMacro(Call const& call)
{
    auto it = macros.find(call.name);
    if (it == macros.end()) {
        throw parse_error(fmt::format("Undefined macro {}", call.name));
    }

    auto saved = save();

    auto m = macros[call.name];
    Symbols shadowed;
    std::vector<std::string> args;
    for (auto const& a : m.args) {
        args.emplace_back(a);
    }

    for (unsigned i = 0; i < call.args.size(); i++) {

        auto const& v = syms.get(args[i]);
        if (v.has_value()) {
            parser.errors.push_back(
                {0, 0,
                 fmt::format("Macro '{}' shadows global symbol {}", call.name,
                             m.args[i]),
                 ErrLevel::Warning});
            shadowed[args[i]] = v;
            syms.erase(args[i]);
        }
        syms[args[i]] = call.args[i];
        LOGD("%s = %x", m.args[i],
             (int32_t)std::any_cast<Number>(call.args[i]));
    }

    // LOGI("Parsing '%s'", m.contents);
    auto ll = lastLabel;
    auto pc = mach->getPC();
    std::string macroLabel = "__macro_"s + std::to_string(pc);
    lastLabel = macroLabel;
    if (!parser.parse(m.contents, fileName.c_str())) {
        throw parse_error("Syntax error in macro");
    }
    // LOGI("Parsing done");
    for (unsigned i = 0; i < call.args.size(); i++) {
        syms.erase(args[i]);
    }
    for (auto const& shadow : shadowed) {
        syms[shadow.first] = shadow.second;
    }
    lastLabel = ll;
}
void Assembler::defineMacro(std::string_view name,
                            std::vector<std::string_view> const& args,
                            std::string_view contents)
{
    macros[name] = {name, args, contents};
}
void Assembler::addDefine(std::string_view name,
                          std::vector<std::string_view> const& args,
                          std::string_view contents)
{
    definitions[name] = {name, args, contents};
}

void initMeta(Assembler& ass);
void initFunctions(Assembler& ass);

Assembler::Assembler() : parser(grammar6502)
{
    parser.packrat();
    mach = std::make_shared<Machine>();
    initFunctions(*this);
    initMeta(*this);
    setupRules();
}
void Assembler::setUndefined(std::string const& sym, SVWrap const& sv)
{
    if (utils::find_if(undefined, [&](auto const& u) {
            return u.name == sym;
        }) != undefined.end()) {
        return;
    }
    undefined.push_back({sym, sv.line_info()});
}

void Assembler::setupRules()
{
    using std::any_cast;
    using namespace std::string_literals;

    parser["EnumLine"] = [&](SV& sv) {
        trace(sv);
        std::any v;
        if (sv.size() == 2) v = sv[1];
        return std::pair<std::string_view, std::any>{
            any_cast<std::string_view>(sv[0]), v};
    };
    parser["EnumBlock"] = [&](SV& sv) {
        trace(sv);
        Symbols s;
        Number lastNumber = 0;
        for (int i = 0; i < sv.size(); i++) {
            if (sv[i].has_value()) {
                auto p = any_cast<std::pair<std::string_view, std::any>>(sv[i]);
                if (!p.second.has_value()) {
                    p.second = std::any(lastNumber++);
                }
                s[persist(p.first)] = p.second;
                if (auto* n = any_cast<Number>(&p.second)) {
                    lastNumber = *n + 1;
                }
            }
        }
        return s;
    };

    parser["AssignLine"] = [&](SV& sv) {
        trace(sv);
        LOGD("Assign %s %d", sv[0].type().name(), sv.size());
        if (sv.size() == 2) {
            auto sym = std::string(any_cast<std::string_view>(sv[0]));
            if (sym[0] == '.') {
                sym = std::string(lastLabel) + sym;
            }
            LOGD("Symbol:%s", sym);
            auto res = syms.set(sym, sv[1]);
            if (res == Symbols::Was::DifferentValue) {
                setUndefined(sym, sv);
            }
        } else if (sv.size() == 1) {
            mach->getCurrentSection().pc = number<uint16_t>(sv[0]);
        }
        return sv[0];
    };

    parser["Label"] = [&](SV& sv) {
        trace(sv);
        auto labelv = std::any_cast<std::string_view>(sv[0]);

        std::string label;
        if (labelv == "$" || labelv == "-" || labelv == "+") {
            label = "__special_" + std::to_string(labelNum);
            labelNum++;
        } else {
            label = std::string(labelv);
            if (labelv[0] == '.') {
                if (lastLabel.empty()) {
                    throw parse_error("Local label without global label");
                }
                label = std::string(lastLabel) + label;
            } else {
                lastLabel = labelv;
            }
        }
        // LOGI("Label %s=%x", label, mach->getPC());
        auto res = syms.set(label, any_num(mach->getPC()));
        if (res == Symbols::Was::DifferentValue) {
            // If we redefine a label we must make another pass
            // since this label could have been used with an
            // incorrect value
            // TODO: Detect used symbols to avoid this
            setUndefined(label, sv);
            // LOGI("Symbol %s changed value", label);
        }
        return sv[0];
    };

    parser["FnDef"] = [&](SV& sv) {
        trace(sv);
        auto name = any_cast<std::string_view>(sv[0]);
        auto args = any_cast<std::vector<std::string_view>>(sv[1]);
        return Def{name, args};
    };
    parser["Root"] = [&](SV& sv) {
        trace(sv);
        parseResult = sv[0];
        return sv[0];
    };

    parser["Meta"] = [&](SV& sv) {
        trace(sv);
        // LOGI("%s (%d) '%s'", sv.name(), sv.size(), sv.token());
        int i = 0;
        auto meta = any_cast<std::string_view>(sv[i++]);
        auto text = any_cast<std::string_view>(sv[i++]);
        // Strip trailing spaces
        auto p = text.size() - 1;
        while (p >= 0 && (text[p] == ' ' || text[p] == '\t')) {
            p--;
        }
        if (text.size() > p) {
            text.remove_suffix(text.size() - 1 - p);
        }

        // LOGI("Meta %d %s '%s'", sv.size(), meta, text);
        std::vector<std::string_view> blocks;

        while (i < sv.size()) {
            blocks.push_back(any_cast<std::string_view>(sv[i++]));
            // LOGI("Block '%s'", blocks.back());
        }

        auto it = metaFunctions.find(std::string(meta));
        if (it != metaFunctions.end()) {
            (it->second)(text, blocks);
            return sv[0];
        }

        // throw parse_error(fmt::format("Unknown meta command '{}'", meta));
        throw parse_error(fmt::format("Unknown meta command '{}'", meta));

        // return sv[0];
    };

    parser["MacroCall"] = [&](SV& sv) {
        trace(sv);
        return sv[0];
    };

    parser["FnCall"] = [&](SV& sv) {
        LOGD("MCALL %s %d", sv[0].type().name(), sv.size());
        auto call = any_cast<Call>(sv[0]);
        auto name = std::string(call.name);

        auto it0 = definitions.find(name);
        if (it0 != definitions.end()) {
            return applyDefine(it0->second, call);
        }

        auto it = functions.find(name);
        if (it != functions.end()) {
            return (it->second)(call.args);
        }

        if (scripting.hasFunction(call.name)) {
            return scripting.call(call.name, call.args);
        }

        throw parse_error(fmt::format("Unknown function '{}'", name));
    };

    parser["Call"] = [&](SV& sv) {
        auto name = any_cast<std::string_view>(sv[0]);
        auto args = any_cast<std::vector<std::any>>(sv[1]);
        return Call{name, args};
    };

    parser["CallArgs"] = [&](SV& sv) {
        // LOGD("CallArgs %d %s", sv.size(), sv[0].type().name());
        std::vector<std::any> v;
        for (size_t i = 0; i < sv.size(); i++) {
            v.push_back(sv[i]);
        }
        return v;
    };
    parser["MetaText"] = [&](SV& sv) { return sv.token_view(); };
    parser["ScriptContents"] = [&](SV& sv) { return sv.token_view(); };

    parser["Symbol"] = [&](SV& sv) { return sv.token_view(); };
    parser["DotSymbol"] = [&](SV& sv) { return sv.token_view(); };
    parser["FnArgs"] = [&](SV& sv) { return sv.transform<std::string_view>(); };
    parser["BlockContents"] = [&](SV& sv) {
        trace(sv);
        return sv.token_view();
    };
    parser["Opcode"] = [&](SV& sv) {
        trace(sv);
        return sv.token_view();
    };
    parser["StringContents"] = [](SV& sv) { return sv.token_view(); };

    parser["Test"] = [&](SV& sv) {
        trace(sv);
        auto contents = any_cast<std::string_view>(sv[1]);
        auto name = any_cast<std::string_view>(sv[0]);
        auto res = runTest(name, contents);
        syms.at<Symbols>("tests").at<Symbols>(name) = res;
        return sv[0];
    };

    parser["OpLine"] = [&](SV& sv) {
        trace(sv);
        if (sv.size() > 0 && sv[0].has_value()) {
            auto arg = sv[0];
            if (auto* i = any_cast<Instruction>(&arg)) {

                auto it = macros.find(i->opcode);
                if (it != macros.end()) {
                    Call c{i->opcode};
                    if (i->mode > sixfive::ACC) {
                        c.args.emplace_back(any_num(i->val));
                    }
                    auto sz = it->second.args.size();
                    if ((sz == 0 && c.args.empty()) ||
                        (sz == 1 && c.args.size() == 1)) {
                        applyMacro(c);
                        return std::any();
                    }
                }

                auto res = mach->assemble(*i);
                if (res < 0) {
                    throw parse_error(
                        fmt::format("Illegal instruction '{}'", i->opcode));
                }
            } else if (auto* c = any_cast<Call>(&arg)) {
                applyMacro(*c);
            }
        }
        return std::any();
    };

    parser["Instruction"] = [&](SV& sv) {
        trace(sv);
        auto opcode = std::string(any_cast<std::string_view>(sv[0]));
        opcode = utils::toLower(opcode);
        Instruction instruction{opcode, AdressingMode::NONE, 0};
        if (sv.size() > 1) {
            auto arg = any_cast<Instruction>(sv[1]);
            instruction.mode = arg.mode;
            instruction.val = arg.val;
        }
        return std::any(instruction);
    };

    // Set up the 'Instruction' parsing rules
    static const std::unordered_map<std::string, AdressingMode> modeMap = {
        {"Abs", AdressingMode::ABS},   {"AbsX", AdressingMode::ABSX},
        {"AbsY", AdressingMode::ABSY}, {"Ind", AdressingMode::IND},
        {"IndX", AdressingMode::INDX}, {"IndY", AdressingMode::INDY},
        {"Acc", AdressingMode::ACC},   {"Imm", AdressingMode::IMM},
    };
    auto buildArg = [](SV& sv) -> std::any {
        return Instruction{"", modeMap.at(sv.name()), any_cast<Number>(sv[0])};
    };
    for (auto const& [name, _] : modeMap) {
        parser[name.c_str()] = buildArg;
    }

    parser["LabelRef"] = [&](SV& sv) -> Instruction {
        auto label = sv.token_view();

        std::any val;
        std::string l = std::string(label);
        if (label == "+") {
            l = "__special_" + std::to_string(labelNum);
        } else if (label == "-") {
            l = "__special_" + std::to_string(labelNum - 1);
        }
        val = syms.get(l);
        if (!val.has_value()) {
            setUndefined(l, sv);
            LOGD("%s undefined atm", l);
            val = any_num(mach->getPC());
        }
        return {"", AdressingMode::ABS, any_cast<Number>(val)};
    };

    parser["Decimal"] = [](SV& sv) -> Number {
        return std::stod(sv.c_str(), nullptr);
    };

    parser["Octal"] = [](SV& sv) -> Number {
        return std::stoi(sv.c_str() + 2, nullptr, 8);
    };

    parser["Multi"] = [](SV& sv) -> Number {
        return std::stoi(sv.c_str() + 2, nullptr, 4);
    };

    parser["Binary"] = [](SV& sv) -> Number {
        return std::stoi(sv.c_str() + 2, nullptr, 2);
    };

    parser["HexNum"] = [](SV& sv) -> Number {
        const char* ptr = sv.c_str();
        if (*ptr == '0') ptr++;
        return std::stoi(ptr + 1, nullptr, 16);
    };

    parser["Star"] = [&](SV& sv) -> Number {
        trace(sv);
        return mach->getPC();
    };

    parser["Expression"] = [&](SV& sv) {
        trace(sv);
        if (sv.size() == 1) {
            return sv[0];
        }
        auto result = any_cast<Number>(sv[0]);
        LOGD("Expr %f", result);
        auto ope = any_cast<std::string_view>(sv[1]);
        auto num = any_cast<Number>(sv[2]);
        auto inum = static_cast<uint64_t>(num);
        auto ires = static_cast<uint64_t>(result);
        // clang-format off
        if (ope == "+") result += num;
        else if (ope == "-") result -= num;
        else if (ope == "*") result *= num;
        else if (ope == "/") result /= num;
        else if (ope == "\\") result = static_cast<Number>(ires / inum);
        else if (ope == "%") result = static_cast<Number>(ires % inum);
        else if (ope == ">>") result = static_cast<Number>(ires >> inum);
        else if (ope == "<<") result = static_cast<Number>(ires << inum);
        else if (ope == "&") result = static_cast<Number>(ires & inum);
        else if (ope == "|") result = static_cast<Number>(ires | inum);
        else if (ope == "^") result = static_cast<Number>(ires ^ inum);
        else if (ope == "&&") result = static_cast<Number>(ires && inum);
        else if (ope == "||") result = static_cast<Number>(ires || inum);
        else if (ope == "<") result = static_cast<Number>(result < num);
        else if (ope == ">") result = static_cast<Number>(result > num);
        else if (ope == "<=") result = static_cast<Number>(result <= num);
        else if (ope == ">=") result = static_cast<Number>(result >= num);
        else if (ope == "<=>") result = num>result ? -1 : static_cast<Number>(result > num);
        else if (ope == "==") result = static_cast<Number>(result == num);
        else if (ope == "!=") result = static_cast<Number>(result != num);
        // clang-format on
        return std::any(result);
    };

    parser["Script"] = [&](SV& sv) {
        if (passNo == 0) {
            scripting.add(std::any_cast<std::string_view>(sv[0]));
        }
        return sv[0];
    };

    parser["Index"] = [&](SV& sv) -> std::any {
        trace(sv);
        auto index = number<size_t>(sv[1]);
        std::any vec = sv[0];
        if (any_cast<Number>(&vec) != nullptr) {
            return any_num(0);
        }
        if (auto const* v = any_cast<std::vector<uint8_t>>(&vec)) {

            if (sv.size() >= 3) { // Slice
                size_t a = 0;
                size_t b = v->size();
                if (sv[1].has_value()) {
                    a = number<size_t>(sv[1]);
                } else if (sv[2].has_value()) {
                    b = number<size_t>(sv[2]);
                }
                if (sv.size() > 3 && sv[3].has_value()) {
                    b = number<size_t>(sv[3]);
                }

                // LOGI("Slice %d:%d", a, b);
                std::vector<uint8_t> nv(v->begin() + a, v->begin() + b);
                return std::any(nv);
            }

            return any_num((*v)[index]);
        }
        auto const& v = any_cast<std::vector<Number>>(vec);
        return any_num(v[index]);
    };

    parser["UnOp"] = [&](SV& sv) { return sv.token()[0]; };

    parser["Unary"] = [&](SV& sv) -> Number {
        trace(sv);
        auto ope = any_cast<char>(sv[0]);
        auto num = any_cast<Number>(sv[1]);
        auto inum = number<int64_t>(num);
        switch (ope) {
        case '~':
            return (Number)(~((uint64_t)num) & 0xffffffff);
        case '-':
            return -num;
        case '!':
            return (inum == 0);
        case '<':
            return inum & 0xff;
        case '>':
            return inum >> 8;
        default:
            throw parse_error("Unknown unary operator");
        }
    };

    parser["Operator"] = [&](SV& sv) { return sv.token_view(); };

    parser["Variable"] = [&](SV& sv) {
        std::any val;
        std::vector<std::string> parts;
        std::string full;

        if (sv.token()[0] == '.') {
            parts = {std::string(lastLabel) + sv.token()};
            val = syms.get(parts[0]);
        } else {
            for (auto const& p : sv.transform<std::string_view>()) {
                parts.emplace_back(p);
            }
            val = syms.get(parts);
            if (!val.has_value()) {
                // LOGD("Dot %s failed, try joined symbol", s);
                full = utils::join(parts.begin(), parts.end(), ".");
                val = syms.get(full);
            }
        }
        if (!val.has_value()) {
            full = utils::join(parts.begin(), parts.end(), ".");
            setUndefined(full, sv);
            LOGD("%s undefined atm", full);
            return any_num(mach->getPC());
        }
        return val;
    };
}

std::vector<Error> Assembler::getErrors() const
{
    return parser.errors;
}

bool Assembler::pass(std::string_view const& source)
{
    labelNum = 0;
    mach->clear();
    undefined.clear();
    parser.errors.clear();
    return parser.parse(source, fileName.c_str());
}

bool Assembler::parse_path(utils::path const& p)
{
    currentPath = utils::absolute(p).parent_path();
    utils::File f{p.string()};
    return parse(f.readAllString(), p.string());
}

bool Assembler::parse(std::string_view const& source, std::string const& fname)
{
    finalPass = false;

    fileName = fname;
    while (true) {
        fmt::print("* PASS {}\n", passNo + 1);
        if (!pass(source)) {
            // throw parse_error("Syntax error");
            return false;
        }

        for (auto const& s : mach->getSections()) {
            auto& secsyms = syms.at<Symbols>("section").at<Symbols>(s.name);
            secsyms["start"] = any_num(s.start);
            secsyms["end"] = any_num(s.start + s.data.size());
            secsyms["data"] = s.data;
        }

        auto result = checkUndefined();
        passNo++;
        if (result == DONE) {
            break;
        }
        if (result == PASS) {
            continue;
        }
        for (auto const& u : undefined) {
            parser.errors.push_back(
                {u.line_info.first, u.line_info.second,
                 fmt::format("Undefined symbol: '{}'", u.name)});
            return false;
        }
    }
    finalPass = true;
    fmt::print("* FINAL PASS\n");
    return pass(source);
}

Symbols& Assembler::getSymbols()
{
    return syms;
}
Machine& Assembler::getMachine()
{
    return *mach;
}

void Assembler::printSymbols()
{
    syms.forAll([](std::string const& name, std::any const& val) {
        if (!utils::startsWith(name, "__"))
            fmt::print("{} == {}\n", name, to_string(val));
    });
}
