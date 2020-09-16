#pragma once

#include "defines.h"
#include "script.h"
#include "wrap.h"

#include "any_callable.h"
#include "symbol_table.h"

#include <coreutils/path.h>

#include <string>
#include <unordered_map>
#include <vector>

class Machine;

#ifdef NO_WRAPPER
#    include "peglib.h"
using SV = const peg::SemanticValues;
using Parser = peg::parser;
#else
using SV = const SVWrap;
using Parser = ParserWrapper;
#endif

inline void Check(bool v, std::string const& txt)
{
    if (!v) throw parse_error(txt);
}

class Assembler
{
public:
    Assembler();
    ~Assembler();

    std::vector<Error> getErrors() const;

    bool parse(std::string_view const& source, std::string const& fname = "");
    bool parse_path(utils::path const& p);

    utils::path getCurrentPath() const { return currentPath; }
    SymbolTable& getSymbols();
    Machine& getMachine();
    void printSymbols();
    std::string_view includeFile(std::string_view fileName);

    bool isFinalPass() const { return finalPass; }
    bool isFirstPass() const { return passNo == 0; }
    template <typename FN>
    void registerFunction(std::string const& name, FN const& fn)
    {
        functions[name] = fn;
    }

    void addScript(utils::path const& p) { scripting.load(p); }

    using MetaFn = std::function<void(
        std::string_view, std::vector<std::string_view> const&, size_t)>;

    using MetaFnNoLine = std::function<void(
        std::string_view, std::vector<std::string_view> const&)>;

    inline void registerMeta(std::string const& name, MetaFn const& fn)
    {
        metaFunctions[name] = fn;
    }

    inline void registerMeta(std::string const& name, MetaFnNoLine const& fn)
    {
        metaFunctions[name] = [fn](std::string_view text,
                                   std::vector<std::string_view> const& blocks,
                                   size_t) { fn(text, blocks); };
    }

    struct Def
    {
        std::string_view name;
        std::vector<std::string_view> args;
    };

    std::any evaluateExpression(std::string_view expr, size_t line = 0);
    Def evaluateDefinition(std::string_view expr, size_t line = 0);
    std::vector<std::any> evaluateList(std::string_view expr, size_t line = 0);
    void defineMacro(std::string_view name,
                     std::vector<std::string_view> const& args, size_t line,
                     std::string_view contents);
    void addDefine(std::string_view name,
                   std::vector<std::string_view> const& args, size_t line,
                   std::string_view contents);

    AnyMap evaluateEnum(std::string_view expr, size_t line = 0);
    void evaluateBlock(std::string_view block, std::string_view fileName = "");
    void evaluateBlock(std::string_view block, size_t line);
    AnyMap runTest(std::string_view name, std::string_view contents);

    enum DebugFlags
    {
        DEB_TRACE = 1,
        DEB_PASS = 2
    };

    void setDebugFlags(uint32_t flags);

    void addCheck(std::string_view text);
    void addLog(std::string_view text);

    uint32_t testLocation = 0xf800;

    utils::path evaluatePath(std::string_view name);
private:
    template <typename T>
    T& sym(std::string const& s)
    {
        return syms.get<T>(s);
    }

    void setRegSymbols();

    std::vector<Error> errors;

    bool passDebug = false;
    struct Call
    {
        std::string_view name;
        std::vector<std::any> args;
    };

    struct Macro
    {
        std::string_view name;
        std::vector<std::string_view> args;
        std::string_view contents;
        size_t line;
    };

    std::unordered_map<std::string, AnyCallable> functions;
    std::unordered_map<std::string, MetaFn> metaFunctions;

    enum
    {
        DONE,
        PASS,
        ERROR
    };

    void applyMacro(Call const& call);
    std::any applyDefine(Macro const& fn, Call const& call);
    int checkUndefined();
    bool pass(std::string_view const& source);
    void setupRules();

    auto save() { return std::tuple(macros, syms, lastLabel); }

    template <class T>
    void restore(T const& t)
    {
        std::tie(macros, syms, lastLabel) = t;
    }

    void trace(SVWrap const& sv) const;

    bool doTrace = false;

    std::unordered_map<int32_t, std::string> checks;
    std::unordered_map<int32_t, std::string> logs;

    utils::path currentPath;
    std::unordered_map<std::string, std::string> includes;
    std::shared_ptr<Machine> mach;
    std::unordered_map<std::string_view, Macro> macros;
    std::unordered_map<std::string_view, Macro> definitions;
    SymbolTable syms;
    std::string_view lastLabel;
    bool finalPass{false};
    int passNo{0};

    std::string fileName;

    Parser parser;

    int labelNum = 0;
    int inMacro = 0;

    int inTest = 0;

    std::any parseResult;

    Scripting scripting;
};
