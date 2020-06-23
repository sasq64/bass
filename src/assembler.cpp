
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
        auto in = static_cast<int64_t>(*n);
        if (*n == in) return fmt::format("${:x}", in);
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
    for (size_t i = 0; i < sv.size(); i++) {
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
    if (syms.done()) return DONE;

    if (!syms.ok()) {
        for (auto const& u : syms.undefined) {
            fmt::format("Undefined symbol {} in line {}", u.first, u.second);
        }
        return ERROR;
    }
    return PASS;
}

Number operator"" _N(unsigned long long a)
{
    return static_cast<Number>(a);
}

AnyMap Assembler::runTest(std::string_view name, std::string_view contents)
{
    if (!finalPass) {
        return {
            {"A", 0_N},  {"X", 0_N},  {"Y", 0_N},      {"SR", 0_N},
            {"SP", 0_N}, {"PC", 0_N}, {"cycles", 0_N}, {"ram", mach->getRam()}};
    }

    auto saved = save();
    auto machSaved = mach->saveState();
    auto start = mach->getPC();
    LOGI("Testing %s:%s at pc %x", name, contents, start);
    auto section = mach->getCurrentSection();
    while (true) {
        syms.clear();
        mach->getCurrentSection() = section;
        lastLabel = "__test_" + std::to_string(start);
        if (!parser.parse(contents, fileName.c_str())) {
            throw parse_error("Syntax error in test block");
        }
        LOGD("Parsing done");
        auto result = checkUndefined();
        if (result == DONE) break;
        if (result == PASS) continue;
    }

    mach->assemble({"rts", AdressingMode::NONE, 0});

    auto cycles = mach->run(start);
    fmt::print("*** Test {} : {} cycles\n", name, cycles);

    auto [a, x, y, sr, sp, pc] = mach->getRegs();

    AnyMap res = {{"A", num(a)},           {"X", num(x)},
                  {"Y", num(y)},           {"SR", num(sr)},
                  {"SP", num(sp)},         {"PC", num(pc)},
                  {"cycles", num(cycles)}, {"ram", mach->getRam()}};

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
        // auto const& v = syms.get(args[i]);
        if (syms.is_defined(args[i])) {
            parser.errors.push_back(
                {0, 0,
                 fmt::format("Function '{}' shadows global symbol {}",
                             call.name, fn.args[i]),
                 ErrLevel::Warning});
            shadowed[args[i]] = syms.get(args[i]);
            syms.erase(args[i]);
        }
        syms.set(args[i], call.args[i]);
        LOGD("%s = %x", fn.args[i],
             (int32_t)std::any_cast<Number>(call.args[i]));
    }

    auto res = evaluateExpression(fn.contents);

    for (unsigned i = 0; i < call.args.size(); i++) {
        syms.erase(args[i]);
    }
    for (auto const& shadow : shadowed) {
        syms.set(shadow.first, shadow.second);
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

        if (syms.is_defined(args[i])) {
            parser.errors.push_back(
                {0, 0,
                 fmt::format("Macro '{}' shadows global symbol {}", call.name,
                             m.args[i]),
                 ErrLevel::Warning});
            shadowed[args[i]] = syms.get(args[i]);
            syms.erase(args[i]);
        }
        syms.set(args[i], call.args[i]);
        // LOGD("%s = %x", m.args[i],
        //     (int32_t)std::any_cast<Number>(call.args[i]));
    }

    // LOGI("Parsing '%s'", m.contents);
    auto ll = lastLabel;
    auto pc = mach->getPC();
    std::string macroLabel = "__macro_"s + std::to_string(pc);
    lastLabel = macroLabel;
    inMacro++;
    if (!parser.parse(m.contents, fileName.c_str())) {
        inMacro--;
        throw parse_error("Syntax error in macro");
    }
    inMacro--;
    // LOGI("Parsing done");
    for (unsigned i = 0; i < call.args.size(); i++) {
        syms.erase(args[i]);
    }
    for (auto const& shadow : shadowed) {
        syms.set(shadow.first, shadow.second);
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
void registerLuaFunctions(Assembler& a, Scripting& s);

Assembler::Assembler() : parser(grammar6502), scripting()
{
    parser.packrat();
    mach = std::make_shared<Machine>();
    initFunctions(*this);
    initMeta(*this);
    setupRules();
    registerLuaFunctions(*this, scripting);
}

Assembler::~Assembler()
{
    mach = nullptr;
}

template <typename A, typename B>
A operation(std::string_view const& ope, A const& a, B const& b)
{
    // clang-format off
    if (ope == "+") return a + b;
    if (ope == "-") return a - b;
    if (ope == "*") return a * b;
    if (ope == "/") return a / b;
    if (ope == "\\") return div(a, b);
    if (ope == "%") return a % b;
    if (ope == ">>") return a >> b;
    if (ope == "<<") return a << b;
    if (ope == "&") return a & b;
    if (ope == "|") return a | b;
    if (ope == "^") return a ^ b;
    if (ope == "&&") return a && b;
    if (ope == "||") return a || b;
    if (ope == "<") return a < b;
    if (ope == ">") return a > b;
    if (ope == "<=") return a <= b;
    if (ope == ">=") return a >= b;
    if (ope == "==") return a == b;
    if (ope == "!=") return a != b;
    // clang-format on
    return a;
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
        for (size_t i = 0; i < sv.size(); i++) {
            if (sv[i].has_value()) {
                auto p = any_cast<std::pair<std::string_view, std::any>>(sv[i]);
                if (!p.second.has_value()) {
                    p.second = std::any(lastNumber++);
                }
                s[persist(p.first)] = p.second;
                if (auto* n = any_cast<Number>(&p.second)) {
                    lastNumber = *n + 1.0;
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
            // LOGI("Symbol:%s", sym);
            syms.set(sym, sv[1]);
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
            if (inMacro != 0) throw parse_error("No special labels in macro");
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
        syms.set(label, static_cast<Number>(mach->getPC()));
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
        size_t i = 0;
        auto meta = any_cast<std::string_view>(sv[i++]);
        auto text = any_cast<std::string_view>(sv[i++]);
        // Strip trailing spaces
        auto p = text.size() - 1;
        while (text[p] == ' ' || text[p] == '\t') {
            if (p == 0) break;
            p--;
        }
        if (text.size() > p) {
            text.remove_suffix(text.size() - 1 - p);
        }

        std::vector<std::string_view> blocks;

        while (i < sv.size()) {
            blocks.push_back(any_cast<std::string_view>(sv[i++]));
        }

        auto it = metaFunctions.find(std::string(meta));
        if (it != metaFunctions.end()) {
            (it->second)(text, blocks);
            return sv[0];
        }
        throw parse_error(fmt::format("Unknown meta command '{}'", meta));
    };

    parser["MacroCall"] = [&](SV& sv) {
        trace(sv);
        return sv[0];
    };

    parser["FnCall"] = [&](SV& sv) {
        trace(sv);
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
        syms.set("tests."s + std::string(name), res);
        return sv[0];
    };

    parser["OpLine"] = [&](SV& sv) {
        trace(sv);
        if (sv.size() > 0 && sv[0].has_value()) {
            auto arg = sv[0];
            if (auto* i = any_cast<Instruction>(&arg)) {

                auto it = macros.find(i->opcode);
                if (it != macros.end()) {
                    LOGD("Found macro %s", it->second.name);
                    Call c{i->opcode, {}};
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
            if (inMacro != 0) throw parse_error("No special labels in macro");
            l = "__special_" + std::to_string(labelNum);
        } else if (label == "-") {
            if (inMacro != 0) throw parse_error("No special labels in macro");
            l = "__special_" + std::to_string(labelNum - 1);
        }
        val = syms.get(l);
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

        auto ope = any_cast<std::string_view>(sv[1]);
        auto a = Num(any_cast<Number>(sv[0]));
        auto b = Num(any_cast<Number>(sv[2]));
        auto result = static_cast<Number>(operation(ope, a, b));
        return std::any(result);
    };

    parser["Script"] = [&](SV& sv) {
        if (passNo == 0) {
            scripting.add(std::any_cast<std::string_view>(sv[0]));
        }
        return sv[0];
    };

    parser["Index"] = [&](SV& sv) {
        trace(sv);
        auto index = any_cast<Number>(sv[1]);
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
        auto num = number(sv[1]);
        auto inum = number<int64_t>(num);
        switch (ope) {
        case '~':
            return static_cast<Number>(~(inum)&0xffffffff);
        case '-':
            return -num;
        case '!':
            return static_cast<Number>(inum == 0);
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
        std::string full;

        if (sv.token()[0] == '.') {
            full = std::string(lastLabel) + sv.token();
        } else {
            auto parts = sv.transform<std::string_view>();
            full = utils::join(parts.begin(), parts.end(), ".");
        }

        val = syms.get(full, sv.line_info().first);
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
    syms.clear();
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
        if (passNo >= 10) {
            return false;
        }
        fmt::print("* PASS {}\n", passNo + 1);
        if (!pass(source)) {
            // throw parse_error("Syntax error");
            return false;
        }

        for (auto const& s : mach->getSections()) {
            // auto& secsyms = syms.at<Symbols>("section").at<Symbols>(s.name);

            auto prefix = "section."s + std::string(s.name);

            auto start = static_cast<Number>(s.start);
            auto end = static_cast<Number>(s.start + s.data.size());

            syms.set(prefix + ".start", start);
            syms.set(prefix + ".end", end);
            syms.set(prefix + ".data", s.data);
        }

        auto result = checkUndefined();
        passNo++;
        if (result == DONE) {
            break;
        }
        if (result == PASS) {
            continue;
        }
        for (auto const& u : syms.undefined) {
            parser.errors.push_back(
                {(size_t)u.second, 0, // u.line_info.first, u.line_info.second,
                 fmt::format("Undefined symbol: '{}'", u.first)});
        }
        return false;
    }
    finalPass = true;
    fmt::print("* FINAL PASS\n");
    return pass(source);
}

SymbolTable& Assembler::getSymbols()
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
