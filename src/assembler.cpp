#include "assembler.h"
#include "chars.h"
#include "defines.h"
#include "machine.h"
#include "parser.h"

#ifndef _WIN32
#    include <cxxabi.h>
#endif
#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/split.h>
#include <coreutils/text.h>
#include <coreutils/utf8.h>

#include <charconv>
#include <fmt/format.h>
#include <string_view>
#include <unordered_set>
extern char const* const grammar6502;

using namespace std::string_literals;
using sixfive::Mode;

// OpenBSD
#ifdef _N
#    undef _N
#endif

Number operator"" _N(unsigned long long a)
{
    return static_cast<Number>(a);
}
Number to_number(std::string_view txt)
{
    std::string const t{txt};
    return static_cast<Number>(std::stod(t));
}

Number to_number(std::string_view txt, Base base, int skip = 0)
{
    int64_t result{};
    txt.remove_prefix(skip);
    auto [ptr, ec] = std::from_chars(txt.data(), txt.data() + txt.size(),
                                     result, static_cast<int>(base));
    if (ec != std::errc()) {
        throw parse_error("Number conversion");
    }
    return static_cast<Number>(result);
}

std::string any_to_string(std::any const& val)
{
    if (auto const* n = std::any_cast<Number>(&val)) {
        auto in = static_cast<int64_t>(*n);
        if (*n == static_cast<double>(in)) return fmt::format("${:x}", in);
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

#ifdef _WIN32
    return val.type().name();
#else
    int status{};
    const char* realname =
        abi::__cxa_demangle(val.type().name(), nullptr, nullptr, &status);

    return realname;
#endif
}

template <typename T>
std::any Assembler::slice(std::vector<T> const& v, int64_t a, int64_t b)
{
    if (b < 0) {
        b = v.size() + b + 1;
    }
    if (a >= b || b > static_cast<int64_t>(v.size())) {
        if (isFinalPass()) {
            throw parse_error("Slice outside array");
        }
        return any_num(0);
    }

    std::vector<T> const nv(v.begin() + a, v.begin() + b);
    return std::any(nv);
}

template <typename T>
std::any Assembler::index(std::vector<T> const& v, int64_t index)
{
    if (index >= static_cast<int64_t>(v.size())) {
        if (isFinalPass()) {
            throw parse_error("Index outside array");
        }
        return any_num(0);
    }
    return any_num(v[index]);
}

void Assembler::setDebugFlags(uint32_t flags)
{
    auto doTrace = (flags & DEB_TRACE) != 0;
    parser.doTrace(doTrace);
    passDebug = (flags & DEB_PASS) != 0;
    syms.trace = passDebug;
}

fs::path Assembler::evaluatePath(std::string_view name)
{
    auto p = fs::path(name);
    if (p.is_relative()) {
        p = currentPath / p;
    }
    return p;
}

Assembler::Block Assembler::includeFile(std::string_view name)
{
    auto fn = std::string(name);
    auto p = fs::path(fn);
    if (p.is_relative()) {
        p = currentPath / p;
        fn = p.string();
    }

    auto it = includes.find(fn);
    if (it != includes.end()) {
        return it->second;
    }

    utils::File f{fn};
    stored_includes.push_back(f.readAllString());

    std::string_view const source = stored_includes.back();
    auto ast = parser.parse(source, name);
    if (ast == nullptr) {
        throw parse_error("");
    }

    Block const block{source, 1, ast};

    includes[fn] = block;
    return includes.at(fn);
}

void Assembler::evaluateCode(std::string const& source, std::string const& name)
{
    auto ast = parser.parse(source, name);
    if (ast == nullptr) {
        throw parse_error("");
    }
    parser.evaluate(ast);
}

void Assembler::evaluateBlock(Block const& block)
{
    parser.evaluate(block.node);
}

int Assembler::checkUndefined()
{
    auto const& undef = syms.get_undefined();
    if (undef.empty()) return DONE;

    syms.resolve();
    if (!undef.empty()) {
        return ERROR;
    }
    return PASS;
}

void Assembler::addTest(std::string name, uint32_t start, RegState const& regs)
{
    tests.push_back({name, start, regs});
    if (name.empty()) {
        pendingTest = &tests.back();
        return;
    }

    if (passNo == 0) {
        AnyMap const res = {{"A", num(0)},      {"X", num(0)},
                            {"Y", num(0)},      {"SR", num(0)},
                            {"SP", num(0)},     {"PC", num(0)},
                            {"cycles", num(0)}, {"ram", mach->getRam()}};
        syms.set("tests."s + name, res);
    }
}

void Assembler::runTest(Test const& test)
{
    using sixfive::Reg;
    AnyMap res;

    auto start = test.start;

    inTest++;

    mach->setRegs(test.regs);

    auto cycles = mach->run(start);
    if (cycles > 16777200) {
        fmt::print("*** Test {} : did not end!\n", test.name);
        throw parse_error("Test did not end");
    }
    auto ra = mach->getReg(Reg::A);
    auto rx = mach->getReg(Reg::X);
    auto ry = mach->getReg(Reg::Y);

    fmt::print("*** Test '{}' : {} cycles, [A=${:02x} X=${:02x} Y=${:02x}]\n",
               test.name, cycles, ra, rx, ry);

    if (test.name != "unnamed") {
        res = {{"A", num(ra)},
               {"X", num(rx)},
               {"Y", num(ry)},
               {"SR", num(mach->getReg(Reg::SR))},
               {"SP", num(mach->getReg(Reg::SP))},
               {"PC", num(mach->getReg(Reg::PC))},
               {"cycles", num(cycles)},
               {"ram", mach->getRam()}};
        syms.set("tests."s + test.name, res);
    }
}

std::any Assembler::applyDefine(Macro const& fn, Call const& call)
{
    AnyMap shadowed;
    std::vector<std::string> args{fn.args.begin(), fn.args.end()};
    for (unsigned i = 0; i < call.args.size(); i++) {
        if (syms.is_defined(args[i])) {
            errors.emplace_back(
                0, 0,
                fmt::format("Function '{}' shadows global symbol {}", call.name,
                            fn.args[i]),
                ErrLevel::Warning);
            shadowed[args[i]] = syms.get(args[i]);
            syms.erase(args[i]);
        }
        syms.set(args[i], call.args[i]);
    }

    // auto res = evaluateExpression(fn.contents);
    auto res = parser.evaluate(fn.contents.node);

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
        // Look for a function if no macro is found
        auto fit = functions.find(std::string(call.name));
        if (fit != functions.end()) {
            auto res = (fit->second)(call.args);
            syms.erase_all("$");
            syms.set("$", res);
            return;
        }
        throw parse_error(fmt::format("Undefined macro {}", call.name));
    }

    auto const& m = macros[call.name];

    if (m.args.size() != call.args.size()) {
        throw parse_error("Wrong number of arguments");
    }

    std::unordered_map<std::string_view, Symbol> shadowed;

    for (unsigned i = 0; i < call.args.size(); i++) {

        if (auto sym = syms.get_sym(m.args[i])) {
            // if (sym != nullptr) {
            errors.emplace_back(
                0, 0,
                fmt::format("Macro '{}' shadows global symbol {}", call.name,
                            m.args[i]),
                ErrLevel::Warning);
            shadowed[m.args[i]] = *sym;
            syms.erase(m.args[i]);
        }
        syms.set(m.args[i], call.args[i]);
    }

    auto ll = lastLabel;
    auto pc = mach->getPC();
    auto macroLabel = "__macro_"s + std::to_string(pc);
    lastLabel = macroLabel;
    inMacro++;
    parser.evaluate(m.contents.node);
    inMacro--;

    for (unsigned i = 0; i < call.args.size(); i++) {
        syms.erase(m.args[i]);
    }
    for (auto const& shadow : shadowed) {
        syms.set_sym(shadow.first, shadow.second);
    }
    lastLabel = ll;
}
void Assembler::defineMacro(std::string_view name,
                            std::vector<std::string_view> const& args,
                            Block const& block)
{
    macros[name] = {name, args, block};
}

void initMeta(Assembler& assem);
void initFunctions(Assembler& ass);
void registerLuaFunctions(Assembler& text, Scripting& scripting);

void Assembler::setRegSymbols()
{
    using sixfive::Reg;
    syms.erase("A");
    syms.erase("X");
    syms.erase("Y");
    syms.erase("SR");
    syms.erase("SP");
    syms.erase("PC");
    syms.erase("RAM");
    syms.set("A", num(mach->getReg(Reg::A)));
    syms.set("X", num(mach->getReg(Reg::X)));
    syms.set("Y", num(mach->getReg(Reg::Y)));
    syms.set("SR", num(mach->getReg(Reg::SR)));
    syms.set("SP", num(mach->getReg(Reg::SP)));
    syms.set("PC", num(mach->getReg(Reg::PC)));
    syms.set("RAM", mach->getRam());
}

void Assembler::machineLog(std::string_view text)
{
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(fmt::arg("X", mach->getReg(sixfive::Reg::X)));
    store.push_back(fmt::arg("Y", mach->getReg(sixfive::Reg::Y)));
    store.push_back(fmt::arg("A", mach->getReg(sixfive::Reg::A)));
    store.push_back(fmt::arg("SP", mach->getReg(sixfive::Reg::SP)));
    store.push_back(fmt::arg("SR", mach->getReg(sixfive::Reg::SR)));
    fmt::vprint(std::string(text) + "\n", store);
}

Assembler::Assembler() : parser(grammar6502)
{
    lines.resize(0x10000);
    parser.packrat();
    mach = std::make_shared<Machine>();

    checkFunction = [this](uint32_t) {
        auto pc = mach->getReg(sixfive::Reg::PC);
        for (auto const& action : actions[pc]) {
            auto saved = syms;
            setRegSymbols();
            if (std::holds_alternative<Check>(action.action)) {
                auto const& check = std::get<Check>(action.action);
                auto v = parser.evaluate(check.expression.node);
                // evaluateExpression(check.expression, action.line);
                if (!number<bool>(v)) {
                    syms = saved;
                    errors.emplace_back(action.line, 0,
                                        fmt::format("Check '{}' failed",
                                                    check.expression.contents));
                    errors.back().file = fileName;
                    throw parse_error("!check");
                }
            } else if (std::holds_alternative<Log>(action.action)) {
                auto const& log = std::get<Log>(action.action);
                machineLog(log.text);
            } else {
                auto const& fn = std::get<std::function<void()>>(action.action);
                fn();
            }
            syms = saved;
        }
        return false;
    };

    initFunctions(*this);
    initMeta(*this);
    setupRules();
    registerLuaFunctions(*this, scripting);
}

Assembler::~Assembler()
{
    mach = nullptr;
}

void Assembler::useCache(bool on)
{
    parser.use_cache(on);
}

void Assembler::handleLabel(std::any const& lbl)
{
    if (auto const* p =
            std::any_cast<std::pair<std::string_view, int32_t>>(&lbl)) {
        // Indexed symbol: Label is array of values
        if (!syms.is_defined(p->first)) {
            syms.set(p->first, std::vector<Number>{});
        }
        auto& vec = syms.get<std::vector<Number>>(p->first);
        if (static_cast<int32_t>(vec.size()) <= p->second) {
            vec.resize(p->second + 1);
        }
        vec[p->second] = static_cast<Number>(mach->getPC());
        // LOGI("setting %s[%d] -> %d", p->first, p->second, (int)vec[0]);
        return;
    }

    std::string label{std::any_cast<std::string_view>(lbl)};

    if (label == "$" || label == "-" || label == "+") {
        ::Check(inMacro == 0, "No special labels in macro");
        label = "__special_" + std::to_string(labelNum);
        labelNum++;
    } else {
        if (label[0] == '.') {
            ::Check(!lastLabel.empty(), "Local label without global label");
            label = std::string(lastLabel) + label;
        } else {
            lastLabel = std::any_cast<std::string_view>(lbl);
        }
    }
    syms.set(label, static_cast<Number>(mach->getPC()));
    syms.set_final(label);
    if (pendingTest != nullptr) {
        auto* test = pendingTest;
        pendingTest = nullptr;
        if (passNo == 0) {
            AnyMap const res = {{"A", num(0)},      {"X", num(0)},
                                {"Y", num(0)},      {"SR", num(0)},
                                {"SP", num(0)},     {"PC", num(0)},
                                {"cycles", num(0)}, {"ram", mach->getRam()}};
            syms.set("tests."s + label, res);
        }
        ::Check(mach->getPC() == test->start,
                "No label found after anonymous test");
        test->name = label;
    }
}

std::vector<uint8_t> operator+(const std::vector<uint8_t>& lhs,
                               const std::vector<uint8_t>& rhs)
{
    std::vector<uint8_t> result = lhs;
    result.reserve(lhs.size() + rhs.size());
    result.insert(result.end(), rhs.begin(), rhs.end());
    return result;
}

std::vector<Number> operator+(const std::vector<Number>& lhs,
                               const std::vector<Number>& rhs)
{
    std::vector<Number> result = lhs;
    result.reserve(lhs.size() + rhs.size());
    result.insert(result.end(), rhs.begin(), rhs.end());
    return result;
}

template <typename A, typename B>
A do_op(std::string_view ope, A const& a, B const& b)
{
    if (ope == "+") return a + b;
    if (ope == "==") return a == b;
    if (ope == "!=") return a != b;
    if (ope == "-") return a - b;
    if (ope == "*") return a * b;
    if (ope == "/") return a / b;
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
    if (ope == "\\") return div(a, b);
    if (ope == ":") return (a<<16) | b;
    return {};
}

AsmValue operation(std::string_view ope, AsmValue const& a, AsmValue const& b)
{
    return std::visit([&](auto&& a0, auto&& b0) -> AsmValue {
        using A = std::decay_t<decltype(a0)>;
        using B = std::decay_t<decltype(b0)>;
        if constexpr (std::is_same_v<A, B>) {
            if constexpr (std::is_arithmetic_v<A>) {
                return static_cast<A>(do_op(ope, Num(a0), Num(b0)));
            } else {
                if (ope == "+") return a0 + b0;
                if (ope == "==") return static_cast<Number>(a0 == b0);
                if (ope == "!=") return static_cast<Number>(a0 != b0);
            }
        } else if constexpr (std::is_arithmetic_v<B> && !std::is_same_v<A, std::string_view>) {
            if (ope == "*") {
                A res;
                for(int i=0; i<b0; i++) {
                    res.insert(res.end(), a0.begin(), a0.end());
                }
                return res;
            }
        }
        return {};
        }, a, b);
}

void Assembler::setSym(std::string_view sym, std::any val)
{
    if (sym[0] == '.') {
        syms.set(std::string(lastLabel) + sym, val);
    } else if (!scopes.empty()) {
        syms.set(std::string(scopes.back()) + "." + sym, val);
        // LOGI("Prefixed to %s", sym);
    } else {
        syms.set(sym, val);
    }
    syms.set_final(sym);
}

AsmValue to_variant(std::any const& a)
{
    if (auto const* n = std::any_cast<Number>(&a))
    {
        return *n;
    }
    if (auto const* sv = std::any_cast<std::string_view>(&a))
    {
        return *sv;
    }
    if (auto const* v8 = std::any_cast<std::vector<uint8_t>>(&a))
    {
        return *v8;
    }
    if (auto const* vn = std::any_cast<std::vector<Number>>(&a))
    {
        return *vn;
    }
    return {};
}

void Assembler::setupRules()
{
    using std::any_cast;
    using SV = const SemanticValues;
    using namespace std::string_literals;

    parser.before("NonEmptyLine", [this](SV& sv) {
        auto pc = mach->getPC();
        lines[pc & 0xffff] = std::make_pair(sv.file_name(), sv.line());
        return true;
    });

    parser.after("AssignLine", [this](SV& sv) {
        if (sv.size() == 2) {

            if (sv[0].type() == typeid(std::string_view)) {
                setSym(any_cast<std::string_view>(sv[0]), sv[1]);
                /* auto sym = std::string(any_cast<std::string_view>(sv[0])); */
                /* if (sym[0] == '.') { */
                /*     sym = std::string(lastLabel) + sym; */
                /* } */
                /* if (!scopes.empty()) { */
                /*     sym = std::string(scopes.back()) + "." + sym; */
                /*     // LOGI("Prefixed to %s", sym); */
                /* } */
                /* syms.set(sym, sv[1]); */
            } else {
                auto args = any_cast<std::vector<std::string_view>>(sv[0]);
                fmt::print("ARGS {}\n", args.size());
                std::any vec = sv[1];
                size_t i = 0;
                if (auto const* v8 = any_cast<std::vector<uint8_t>>(&vec)) {
                    for(auto&& sym : args) {
                        auto v = i < v8->size() ? (*v8)[i] : 0;
                        syms.set(sym, v);
                        i++;
                    }
                }
                else if (auto const* vn = any_cast<std::vector<Number>>(&vec)) {
                    for(auto&& sym : args) {
                        auto v = i < vn->size() ? (*vn)[i] : 0;
                        setSym(sym, v);
                        i++;
                    }
                } else {
                    // TODO: Also handle lambda and map
                    throw parse_error("Can not destructure non-array");
                }
            }
        } else if (sv.size() == 1) {
            mach->getCurrentSection().pc = number<uint16_t>(sv[0]);
        }
        return sv[0];
    });

    parser.after("DotSymbol",
                 [&](SV& sv) -> std::any { return sv.token_view(); });
    parser.after("AsmSymbol", [&](SV& sv) -> std::any {
        if (sv.size() == 2) {
            // Indexed symbol
            auto n = number<int32_t>(sv[1]);
            auto s = any_cast<std::string_view>(sv[0]);
            return std::make_pair(s, n);
        }
        return sv.token_view();
    });

    parser.after("Label", [this](SV& sv) -> std::any {
        handleLabel(sv[0]);
        return {};
    });

    parser.after("FnDef", [&](SV& sv) {
        auto name = any_cast<std::string_view>(sv[0]);
        auto args = any_cast<std::vector<std::string_view>>(sv[1]);
        return Def{name, args};
    });

    parser.after("MetaName", [&](SV& sv) { return sv[0]; });

    parser.after("GenericDecl", [&](SV& sv) -> std::any {
        Meta meta;
        meta.text = sv.token_view();
        meta.name = any_cast<std::string_view>(sv[0]);
        meta.args = any_cast<std::vector<std::any>>(sv[1]);
        meta.line = sv.line();
        return meta;
    });

    parser.after("IfBlock", [&](SV& sv) -> std::any {
        Meta meta;
        meta.args.emplace_back(any_cast<Number>(sv[0]));
        for (size_t i = 1; i < sv.size(); i++) {
            meta.blocks.push_back(std::any_cast<Block>(sv[i]));
        }
        meta.name = "if";
        meta.line = sv.line();
        return meta;
    });

    parser.after("IfDefDecl", [this](SV& sv) -> std::any {
        ::Check(sv.size() >= 1, "Invalid !ifdef declaration");
        auto s = any_cast<std::string_view>(sv[0]);
        return Number(syms.is_defined(s));
    });

    parser.after("IfNDefDecl", [this](SV& sv) -> std::any {
        ::Check(sv.size() >= 1, "Invalid !ifndef declaration");
        auto s = any_cast<std::string_view>(sv[0]);
        return Number(!syms.is_defined(s));
    });

    parser.after("CheckDecl", [&](SV& sv) -> std::any {
        ::Check(sv.size() >= 1, "Invalid !check declaration");
        Meta meta;
        meta.name = "check";
        meta.blocks.push_back(any_cast<Block>(sv[0]));
        meta.line = sv.line();
        return meta;
    });

    parser.before("DelayedExpression", [](SV&) -> bool {
        return false; // Dont descend into children
    });

    parser.after("DelayedExpression", [&](SV& sv) -> std::any {
        // Save child 'Expression' node for later evaluation
        return Block{sv.token_view(), sv.line(), get_child(sv.get_node(), 0)};
    });

    parser.after("MacroDecl", [&](SV& sv) -> std::any {
        ::Check(sv.size() >= 1, "Invalid !macro declaration");
        auto fndef = any_cast<Def>(sv[0]);
        Meta meta;

        meta.name = "macro";
        meta.text = sv.token_view();
        meta.args.emplace_back(fndef.name);
        meta.args.emplace_back(fndef.args);
        meta.line = sv.line();

        return meta;
    });

    parser.after("MetaBlock", [this](SV& sv) {
        size_t i = 0;

        if (!sv[i].has_value()) {
            // Skip label
            i++;
        }
        auto meta = any_cast<Meta>(sv[i++]);
        while (i < sv.size()) {
            if (sv[i].type() == typeid(Block)) {
                auto block = any_cast<Block>(sv[i]);
                meta.blocks.push_back(block);
            } else if (sv[i].type() == typeid(std::string_view)) {
                meta.blocks.push_back(
                    {any_cast<std::string_view>(sv[i]), sv.line(), nullptr});
            }
            i++;
        }

        for(auto&& a : meta.args) {
            auto v = to_variant(a);
            meta.vargs.push_back(v);
        }


        if (meta.name.empty()) {
            return sv[0];
        }

        auto it = metaFunctions.find(std::string(meta.name));
        if (it != metaFunctions.end()) {
            try {
                (it->second)(meta);
            } catch (parse_error& e) {
                throw parse_error(e.what());
            }
            return sv[0];
        }
        throw parse_error(fmt::format("Unknown meta command '{}'", meta.name));
    });

    parser.after("MacroCall", [&](SV& sv) { return sv[0]; });

    parser.after("FnCall", [this](SV& sv) {
        ::Check(sv.size() >= 1, "Invalid function call");
        auto call = any_cast<Call>(sv[0]);
        auto name = std::string(call.name);
        bool found = false;

        if (auto sym = syms.get_sym(name)) {
            if (auto const* macro = std::any_cast<Macro>(&sym->value)) {
                return applyDefine(*macro, call);
            }
            found = true;
        }

        auto it = functions.find(name);
        if (it != functions.end()) {
            try {
                return (it->second)(call.args);
            } catch (std::bad_any_cast&) {
                return std::any{};
            }
        }

        if (scripting.hasFunction(call.name)) {
            return scripting.call(call.name, call.args);
        }

        if (found) {
            throw parse_error(fmt::format("'{}' is not callable", name));
        }

        throw parse_error(fmt::format("Unknown function '{}'", name));
    });

    parser.after("Lambda", [&](SV& sv) -> std::any {
        ::Check(sv.size() >= 2, "Invalid lambda expression");
        auto args = any_cast<std::vector<std::string_view>>(sv[0]);
        auto block = any_cast<Block>(sv[1]);
        return Macro{"", args, block};
    });

    parser.after("Lambda2", [&](SV& sv) -> std::any {
        ::Check(sv.size() >= 1, "Invalid lambda expression");
        auto block = any_cast<Block>(sv[0]);
        return Macro{"", {}, block};
    });

    parser.after("Call", [&](SV& sv) {
        auto name = any_cast<std::string_view>(sv[0]);
        auto args = any_cast<std::vector<std::any>>(sv[1]);
        return Call{name, args};
    });

    parser.after("CallArgs", [&](SV& sv) {
        std::vector<std::any> v;
        v.reserve(sv.size());
        for (size_t i = 0; i < sv.size(); i++) {
            v.push_back(sv[i]);
        }
        return v;
    });

    parser.after("CallArg", [&](SV& sv) {
        if (sv.size() == 1) {
            return sv[0];
        }
        return std::any(
            std::make_pair(std::any_cast<std::string_view>(sv[0]), sv[1]));
    });

    parser.after("ScriptContents", [&](SV& sv) { return sv.token_view(); });
    parser.after("ScriptContents2", [&](SV& sv) { return sv.token_view(); });

    parser.after("Symbol", [&](SV& sv) { return sv.token_view(); });
    parser.after("Dollar", [&](SV& sv) { return sv.token_view(); });
    parser.after("FnArgs", [&](SV& sv) {
        std::vector<std::string_view> parts;
        parts.reserve(sv.size());
        for (size_t i = 0; i < sv.size(); i++) {
            parts.emplace_back(any_cast<std::string_view>(sv[i]));
        }
        return parts;
    });
    parser.after("DArgs", [&](SV& sv) {
        std::vector<std::string_view> parts;
        parts.reserve(sv.size());
        for (size_t i = 0; i < sv.size(); i++) {
            parts.emplace_back(any_cast<std::string_view>(sv[i]));
        }
        return parts;
    });

    parser.before("BlockProgram", [&](SV&) { return false; });

    parser.after("BlockProgram", [&](SV& sv) -> std::any {
        return Block{sv.token_view(), sv.line(), get_child(sv.get_node(), 0)};
    });

    parser.after("EnumLine", [this](SV& sv) -> std::any {
        if (sv.size() == 0) {
            // Empty line
            return {};
        }
        auto sym = any_cast<std::string_view>(sv[0]);
        Number value = nextEnumValue;

        if (sv.size() > 1) {
            value = any_cast<Number>(sv[1]);
        }
        nextEnumValue = value + 1;
        return std::pair(sym, value);
    });

    parser.after("EnumBlock", [this](SV& sv) -> std::any {
        AnyMap m;
        nextEnumValue = 0;
        if (!sv[0].has_value()) {
            for (size_t i = 1; i < sv.size(); i++) {
                if (sv[i].has_value()) {
                    auto const& [name, value] =
                        any_cast<std::pair<std::string_view, Number>>(sv[i]);
                    syms[name] = value;
                }
            }
            return Meta{};
        }
        auto sym = any_cast<std::string_view>(sv[0]);
        for (size_t i = 1; i < sv.size(); i++) {
            if (sv[i].has_value()) {
                auto const& [name, value] =
                    any_cast<std::pair<std::string_view, Number>>(sv[i]);
                m[std::string(name)] = value;
            }
        }

        syms.set(sym, m);

        return Meta{};
    });

    parser.after("Opcode", [&](SV& sv) {
        std::string_view suffix;
        auto name = any_cast<std::string_view>(sv[0]);
        if (sv.size() == 2) {
            suffix = any_cast<std::string_view>(sv[1]);
        }
        return std::pair(name, suffix);
    });
    parser.after("StringContents", [](SV& sv) { return sv.token_view(); });

    parser.after("Suffix", [](SV& sv) { return sv.token_view(); });

    parser.after("OpLine", [this](SV& sv) {
        for (size_t n = 0; n < sv.size(); n++) {
            auto arg = sv[n];
            if (auto* i = any_cast<Instruction>(&arg)) {
                auto it = macros.find(i->opcode);
                if (it != macros.end()) {
                    LOGD("Found macro %s", it->second.name);
                    Call c{i->opcode, {}};
                    if (i->mode > sixfive::Mode::ACC) {
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
                if (res == AsmResult::Truncated && !isFinalPass()) {
                    // Accept long branches unless final pass
                    res = AsmResult::Ok;
                }
                if (res != AsmResult::Ok) {
                    throw parse_error(
                        fmt::format("Illegal instruction '{}'", i->opcode));
                }
            } else if (auto* c = any_cast<Call>(&arg)) {
                applyMacro(*c);
            }
        }
        return std::any();
    });

    parser.after("Instruction", [&](SV& sv) {
        auto [opcode, suffix] =
            any_cast<std::pair<std::string_view, std::string_view>>(sv[0]);
        // opcode = utils::toLower(opcode);
        Instruction instruction{opcode, Mode::NONE, 0};
        if (sv.size() > 1) {
            auto arg = any_cast<Instruction>(sv[1]);
            if (arg.mode == sixfive::Mode::ADR && suffix == "b") {
                arg.mode = sixfive::Mode::ZP;
            }
            if (arg.mode == sixfive::Mode::ADR && suffix == "w") {
                arg.mode = sixfive::Mode::ABS;
            }
            instruction.mode = arg.mode;
            instruction.val = arg.val;
        }
        return std::any(instruction);
    });

    // Set up the 'Instruction' parsing rules
    static const std::unordered_map<std::string, Mode> modeMap = {
        {"Abs", Mode::ADR}, {"AbsX", Mode::ADRX}, {"AbsY", Mode::ADRY},
        {"Ind", Mode::IND}, {"IndX", Mode::INDX}, {"IndY", Mode::INDY},
        {"Acc", Mode::ACC}, {"Imm", Mode::IMM},
    };
    auto buildArg = [](SV& sv) -> std::any {
        auto mode = modeMap.at(std::string(sv.name()));
        return Instruction{"", mode,
                           mode == Mode::ACC ? 0 : any_cast<Number>(sv[0])};
    };
    for (auto const& [name, _] : modeMap) {
        parser.after(name.c_str(), buildArg);
    }

    parser.after("ZRel", [](SV& sv) -> Instruction {
        auto v = (number<int32_t>(sv[1]) << 24) |
                 (number<int32_t>(sv[0]) << 16) | number<int32_t>(sv[2]);
        return {"", Mode::ZP_REL, static_cast<Number>(v)};
    });

    parser.after("LabelRef", [this](SV& sv) {
        auto label = sv.token_view();

        std::string l = std::string(label);
        if (label[0] == '+') {
            if (inMacro != 0) throw parse_error("No special labels in macro");
            l = "__special_" + std::to_string(labelNum + label.length() - 1);
        } else if (label[0] == '-') {
            if (inMacro != 0) throw parse_error("No special labels in macro");
            l = "__special_" + std::to_string(labelNum - label.length());
        }
        return syms.get(l);
    });

    parser.after("Decimal", [this](SV& sv) -> Number {
        try {
            return to_number(sv.token_view());
        } catch (std::out_of_range&) {
            if (isFinalPass()) {
                throw parse_error("Out of range");
            }
            return 0;
        }
    });

    parser.after("Octal", [this](SV& sv) -> Number {
        try {
            return to_number(sv.token_view(), Base::Octal, 2);
        } catch (std::out_of_range&) {
            if (isFinalPass()) {
                throw parse_error("Out of range");
            }
            return 0;
        }
    });

    parser.after("Multi", [this](SV& sv) -> Number {
        try {
            return to_number(sv.token_view(), Base::Quad, 2);
        } catch (std::out_of_range&) {
            if (isFinalPass()) {
                throw parse_error("Out of range");
            }
            return 0;
        }
    });

    parser.after("Binary", [this](SV& sv) -> Number {
        try {
            int skip = 1;
            if (sv.token_view()[0] == '0') skip++;
            return to_number(sv.token_view(), Base::Binary, skip);
        } catch (std::out_of_range&) {
            if (isFinalPass()) {
                throw parse_error("Out of range");
            }
            return 0;
        }
    });

    parser.after("Char", [&](SV& sv) -> Number {
        auto t = sv.token_view();
        t.remove_suffix(1);
        t.remove_prefix(1);
        auto s = utils::utf8_decode(t);
        if (s.size() != 1) {
            throw parse_error("Illegal character literal");
        }
        return s[0];
    });

    parser.after("HexNum", [this](SV& sv) -> Number {
        try {
            int skip = 1;
            if (sv.token_view()[0] == '0') skip++;
            return to_number(sv.token_view(), Base::Hex, skip);
        } catch (std::out_of_range&) {
            if (isFinalPass()) {
                throw parse_error("Out of range");
            }
            return 0;
        }
    });

    parser.after("ArrayLiteral", [](SV& sv) -> std::vector<Number> {
        std::vector<Number> v;
        for (size_t i = 0; i < sv.size(); i++) {
            std::any const& a = sv[i];
            v.push_back(number(a));
        }
        return v;
    });

    parser.after("Star", [this](SV&) -> Number { return mach->getPC(); });

    parser.after("Expression", [this](SV& sv) {
        if (sv.size() == 2) {
            // Tern
            auto p = any_cast<std::pair<Block, Block>>(sv[1]);
            if (number(sv[0]) != 0) {
                return parser.evaluate(p.first.node);
            }
            return parser.evaluate(p.second.node);
        }
        return sv[0];
    });

    parser.after("Tern", [&](SV& sv) -> std::any {
        return std::make_pair(std::any_cast<Block>(sv[0]),
                              std::any_cast<Block>(sv[1]));
    });

    parser.after("Expression2", [this](SV& sv) {
        if (sv.size() == 1) {
            return sv[0];
        }

        auto ope = any_cast<std::string_view>(sv[1]);
        auto arg1 = to_variant(sv[0]);
        auto arg2 = to_variant(sv[2]);

      try {
          auto res = operation(ope, arg1, arg2);
          return std::visit([&](auto&& r) { return std::any(r); }, res);
      } catch (std::out_of_range&) {
          if (isFinalPass()) {
              throw parse_error("Out of range");
          }
          return any_num(0);
      } catch (dbz_error&) {
          if (isFinalPass()) {
              throw parse_error("Division by zero");
          }
          return any_num(0);
      }
    });

    parser.after("Script", [this](SV& sv) {
        if (passNo == 0) {
            scripting.add(std::any_cast<std::string_view>(sv[0]));
        }
        return sv[0];
    });

    parser.after("Index", [this](SV& sv) -> std::any {
        if (sv.size() == 1) {
            return sv[0];
        }
        std::any vec = sv[0];
        if (any_cast<Number>(&vec) != nullptr) {
            // Slicing undefined symbol, return 0
            return any_num(0);
        }

        if (sv.size() >= 3) { // Slice
            int64_t a = 0;
            int64_t b = -1;
            if (sv[1].has_value()) {
                a = number<int64_t>(sv[1]);
            } else if (sv[2].has_value()) {
                b = number<int64_t>(sv[2]);
            }
            if (sv.size() > 3 && sv[3].has_value()) {
                b = number<int64_t>(sv[3]);
            }
            if (auto const* v8 = any_cast<std::vector<uint8_t>>(&vec)) {
                return slice(*v8, a, b);
            }
            if (auto const* vn = any_cast<std::vector<Number>>(&vec)) {
                return slice(*vn, a, b);
            }

            // Slice lambda
            if (auto const* macro = any_cast<Macro>(&vec)) {
                std::vector<Number> result(b - a);
                Call call;
                call.args.resize(1);
                for (int64_t i = a; i < b; i++) {
                    call.args[0] = any_num(i);
                    auto res = applyDefine(*macro, call);
                    result[i - a] = number<uint8_t>(res);
                }
                return result;
            }

            throw parse_error("Can not slice non-array");
        }

        auto i = number<int64_t>(sv[1]);
        if (auto const* v8 = any_cast<std::vector<uint8_t>>(&vec)) {
            return index(*v8, i);
        }
        if (auto const* vn = any_cast<std::vector<Number>>(&vec)) {
            return index(*vn, i);
        }
        throw parse_error("Can not index non-array");
    });

    parser.after("UnOp", [&](SV& sv) { return sv.token_view()[0]; });
    parser.after("UnOp2", [&](SV& sv) { return sv.token_view()[0]; });

    parser.after("Unary", [&](SV& sv) -> Number {
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
        default:
            throw parse_error("Unknown unary operator");
        }
    });
    parser.after("Unary2", [&](SV& sv) -> Number {
        auto ope = any_cast<char>(sv[0]);
        auto num = number(sv[1]);
        auto inum = number<int64_t>(num);
        switch (ope) {
        case '<':
            return static_cast<Number>(inum & 0xff);
        case '>':
            return static_cast<Number>(inum >> 8);
        default:
            throw parse_error("Unknown unary operator");
        }
    });

    parser.after("Operator", [&](SV& sv) { return sv.token_view(); });

    parser.after("Variable", [this](SV& sv) {
        std::any val;
        std::string full;

        if (sv.token_view() == "true") {
            return any_num(1);
        }
        if (sv.token_view() == "false") {
            return any_num(0);
        }

        if (sv.token_view()[0] == '.') {
            full = std::string(lastLabel) + sv.token_view();
        } else {

            std::vector<std::string_view> parts;
            parts.reserve(sv.size());
            for (size_t i = 0; i < sv.size(); i++) {
                parts.emplace_back(any_cast<std::string_view>(sv[i]));
            }
            full = utils::join(parts.begin(), parts.end(), ".");
        }

        val = syms.get(full);
        // Set undefined numbers to PC, to increase likelihood of
        // correct code generation (fewer passes)
        if (val.type() == typeid(Number) && !syms.is_defined(full)) {
            val = static_cast<Number>(mach->getPC());
        }
        return val;
    });
}

std::vector<Error> Assembler::getErrors() const
{
    return errors;
}

bool Assembler::pass(AstNode const& ast)
{
    labelNum = 0;
    mach->clear();
    syms.clear();
    errors.clear();
    tests.clear();
    actions.clear();
    needsFinalPass = false;
    try {
        parser.evaluate(ast);
    } catch (std::bad_any_cast&) {
        Error error = parser.getError();
        error.message = "Data type error";
        errors.push_back(error);
        return false;
    } catch (script_error& e) {
        std::string w = e.what();
        auto r = w.find("]:");
        auto colon = w.find(':', r + 2);
        if (r != std::string::npos && colon != std::string::npos) {
            auto ln = std::stol(w.substr(r + 2), nullptr, 10);
            w = "lua: "s + w.substr(colon + 2);
            Error error = parser.getError();
            error.line += (ln - 1);
            error.message = w;
            errors.push_back(error);
        }
        return false;
    } catch (std::exception&) {
        errors.push_back(parser.getError());
        return false;
    }
    return true;
}

bool Assembler::parse_path(fs::path const& p)
{
    currentPath = fs::absolute(p).parent_path();
    utils::File f{p.string()};

    return parse(f.readAllString() + "\n", p.string());
}

bool Assembler::parse(std::string_view source, std::string const& fname)
{
    finalPass = false;
    const char* bom = "\xef\xbb\xbf";

    if (utils::startsWith(source, bom)) {
        // BOM
        source.remove_prefix(3);
    }

    fileName = fname;

    fmt::print("* PARSING\n");
    auto ast = parser.parse(source, fname);
    if (!ast) {
        errors.push_back(parser.getError());
        return false;
    }

    syms.accept_undefined(true);
    while (true) {
        if (passNo >= maxPasses) {
            errors.emplace_back(0, 0, "Max number of passes");
            return false;
        }
        fmt::print("* PASS {}\n", passNo + 1);
        if (!pass(ast)) {
            // throw parse_error("Syntax error");
            return false;
        }

        auto layoutOk = mach->layoutSections();

        for (auto const& s : mach->getSections()) {
            LOGD("%s : %x -> %x (%d) [%x]\n", s.name, s.start, s.start + s.size,
                 s.data.size(), s.flags);

            auto prefix = "section."s + std::string(s.name);

            auto start = static_cast<Number>(s.start);
            auto end = static_cast<Number>(s.start + s.data.size());

            syms.set(prefix + ".start", start);
            syms.set(prefix + ".end", end);
            syms.set(prefix + ".data", s.data);
        }

        passNo++;
        auto rc = checkUndefined();

        if (rc == PASS) {
            continue;
        }
        if (rc == ERROR) {
            tests.clear();
            needsFinalPass = true;
        }

        if (!layoutOk) {
            continue;
        }
        break;
    }
    auto err = mach->checkOverlap();
    if (err.line != 0) {
        err.file = fileName;
        errors.push_back(err);
        return false;
    }

    if (!tests.empty()) {
        fmt::print("* TESTS ({})\n", tests.size());
        try {
            for (auto const& test : tests) {
                runTest(test);
            }
        } catch (parse_error& e) {
            LOGE("Error %s", e.what());
            return false;
        }
    }

    if (needsFinalPass) {
        finalPass = true;
        fmt::print("* FINAL PASS\n");
        syms.accept_undefined(false);
        return pass(ast);
    }
    return true;
}

SymbolTable<double>& Assembler::getSymbols()
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
            fmt::print("{} == {}\n", name, any_to_string(val));
    });
}

void Assembler::writeSymbols(fs::path const& p)
{
    auto f = createFile(p);
    syms.forAll([&](std::string const& name, std::any const& val) {
        if (!utils::startsWith(name, "__") &&
            (name.find('.') == std::string::npos) &&
            val.type() == typeid(Number))
            fmt::print(f.filePointer(), "{} = {}\n", name, any_to_string(val));
    });
}

void Assembler::addRunnable(std::string_view text, size_t line)
{
    auto fn = scripting.make_function(text);
    auto& action = actions[mach->getPC()];
    action.emplace_back(EmuAction{line, fn});
    getMachine().addIntercept(mach->getPC(), checkFunction);
}

void Assembler::addLog(std::string_view text, size_t line)
{
    auto& action = actions[mach->getPC()];
    action.emplace_back(EmuAction{line, Log{text}});
    getMachine().addIntercept(mach->getPC(), checkFunction);
}

void Assembler::addCheck(Block const& block, size_t line)
{
    auto& action = actions[mach->getPC()];
    action.emplace_back(EmuAction{line, Check{block}});
    getMachine().addIntercept(mach->getPC(), checkFunction);
}

void Assembler::pushScope(std::string_view name)
{
    scopes.push_back(name);
}

void Assembler::popScope()
{
    scopes.pop_back();
}

void Assembler::clear()
{
    macros.clear();
    errors.clear();
    passNo = 0;
}

Assembler::MetaFn Assembler::getMetaFn(const std::string& name)
{
    return metaFunctions[name];
}
