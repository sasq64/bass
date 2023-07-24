#pragma once

#include "defines.h"
#include "parser.h"
#include "script.h"

#include "any_callable.h"
#include "symbol_table.h"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class Machine;

inline void Check(bool v, std::string const& txt)
{
    if (!v) throw parse_error(txt);
}

enum class Base
{
    Hex = 16,
    Decimal = 10,
    Octal = 8,
    Quad = 4,
    Binary= 2
};

class Assembler
{

public:
    Assembler();
    ~Assembler();

    struct Test
    {
        std::string name;
        uint32_t start;
        RegState regs;
    };

    struct Block //NOLINT
    {
        std::string_view contents;
        size_t line;
        std::shared_ptr<peg::AstBase<BassNode>> node;
    };

    struct Macro
    {
        std::string_view name;
        std::vector<std::string_view> args;
        Block contents;
    };

    struct Call
    {
        std::string_view name;
        std::vector<std::any> args;
    };

    void handleLabel(std::any const& lbl);

    void pushScope(std::string_view name);
    void popScope();

    void machineLog(std::string_view text);

    std::vector<Error> getErrors() const;

    bool parse(std::string_view source, std::string const& fname = "");
    bool parse_path(fs::path const& p);

    fs::path getCurrentPath() const { return currentPath; }
    void setCurrentPath(fs::path const& p) { currentPath = p; }
    SymbolTable<double>& getSymbols();
    Machine& getMachine();
    void printSymbols();
    void writeSymbols(fs::path const& p);
    Block includeFile(std::string_view fileName);

    void setMaxPasses(int mp) { maxPasses = mp; }

    bool isFinalPass()
    {
        needsFinalPass = true;
        return finalPass;
    }
    bool isFirstPass() const { return passNo == 0; }
    template <typename FN>
    void registerFunction(std::string const& name, FN const& fn)
    {
        functions[name] = fn;
    }

    void addScript(fs::path const& p) { scripting.load(p); }

    struct Meta
    {
        std::string text;
        std::string_view name;
        std::vector<std::any> args;
        std::vector<AsmValue> vargs;
        std::vector<Block> blocks;
        size_t line = 0;
    };

    using MetaFn = std::function<void(Meta const&)>;

    inline void registerMeta(std::string const& name, MetaFn const& fn)
    {
        metaFunctions[name] = fn;
        metaFunctions[utils::toUpper(name)] = fn;
    }

    struct Def
    {
        std::string_view name;
        std::vector<std::string_view> args;
    };

    void defineMacro(std::string_view name,
                     std::vector<std::string_view> const& args,
                     Block const& block);

    void evaluateBlock(Block const& block);

    void runTest(Test const& test);
    void addTest(std::string name, uint32_t start, RegState const& regs);

    enum DebugFlags
    {
        DEB_TRACE = 1,
        DEB_PASS = 2
    };

    void setDebugFlags(uint32_t flags);

    void addCheck(Block const& block, size_t line);
    void addLog(std::string_view text, size_t line);
    void addRunnable(std::string_view text, size_t line);

    void evaluateCode(std::string const& source, std::string const& name);
    fs::path evaluatePath(std::string_view name);
    std::string_view getLastLabel() const { return lastLabel; }
    void setLastLabel(std::string_view l) { lastLabel = l; }
    void setLastLabel(std::string const& l) { lastLabel = persist(l); }

    std::any applyDefine(Macro const& fn, Call const& call);

    void clear();

    void useCache(bool on);

    std::vector<std::pair<std::string, int>> const& getLines() const { return lines; }

    Assembler::MetaFn getMetaFn(const std::string& name);

private:
    template <typename T>
    T& sym(std::string const& s)
    {
        return syms.get<T>(s);
    }

    template <typename T>
    std::any slice(std::vector<T> const& v, int64_t a, int64_t b);
    template <typename T>
    std::any index(std::vector<T> const& v, int64_t index);

    void setSym(std::string_view sym, std::any val);
    void setRegSymbols();

    std::vector<Error> errors;

    bool passDebug = false;
    std::unordered_map<std::string, AnyCallable> functions;
    std::unordered_map<std::string, MetaFn> metaFunctions;

    enum
    {
        DONE,
        PASS,
        ERROR
    };

    void applyMacro(Call const& call);
    int checkUndefined();
    bool pass(AstNode const& ast);
    void setupRules();

    auto save() { return std::tuple(macros, syms, lastLabel); }

    template <class T>
    void restore(T const& t)
    {
        std::tie(macros, syms, lastLabel) = t;
    }

    struct Check
    {
        Block expression;
    };

    struct Log
    {
        std::string_view text;
    };

    struct EmuAction
    {
        size_t line;
        std::variant<Check, Log, std::function<void()>> action;
    };

    std::unordered_map<uint32_t, std::vector<EmuAction>> actions;

    fs::path currentPath;
    std::unordered_map<std::string, Block> includes;
    std::deque<std::string> stored_includes;
    std::shared_ptr<Machine> mach;
    std::unordered_map<std::string_view, Macro> macros;
    SymbolTable<double> syms;
    std::string_view lastLabel;
    bool finalPass{false};
    bool needsFinalPass{false};
    int passNo{0};

    std::vector<std::pair<std::string, int>> lines;

    std::string fileName;

    Parser parser;

    int labelNum = 0;
    int inMacro = 0;

    int inTest = 0;
    int maxPasses = 10;

    std::function<bool(uint32_t)> logFunction;
    std::function<bool(uint32_t)> checkFunction;

    Test* pendingTest = nullptr;
    std::vector<Test> tests;

    std::deque<std::string_view> scopes;

    Number nextEnumValue = 0;

    Scripting scripting;
};
