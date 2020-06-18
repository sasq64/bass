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

class Assembler
{
public:
    Assembler();

    std::vector<Error> getErrors() const;

    bool parse(std::string_view const& source, std::string const& fname = "");
    bool parse_path(utils::path const& p);

    utils::path getCurrentPath() const { return currentPath; }
    Symbols& getSymbols();
    Machine& getMachine();
    void printSymbols();
    std::string_view includeFile(std::string_view fileName);

    bool isFinalPass() const { return finalPass; }
    template <typename FN>
    void registerFunction(std::string const& name, FN const& fn)
    {
        functions[name] = fn;
    }

    void addScript(utils::path const& p) { scripting.load(p); }

    using MetaFn = std::function<void(std::string_view,
                                      std::vector<std::string_view> const&)>;

    void registerMeta(std::string const& name, MetaFn const& fn)
    {
        metaFunctions[name] = fn;
    }

    struct Def
    {
        std::string_view name;
        std::vector<std::string_view> args;
    };

    std::any evaluateExpression(std::string_view expr);
    Def evaluateDefinition(std::string_view expr);
    std::vector<std::any> evaluateList(std::string_view expr);
    void defineMacro(std::string_view name,
                     std::vector<std::string_view> const& args,
                     std::string_view contents);
    void addDefine(std::string_view name,
                   std::vector<std::string_view> const& args,
                   std::string_view contents);

    Symbols evaluateEnum(std::string_view expr);
    void evaluateBlock(std::string_view block, std::string_view fileName = "");
    Symbols runTest(std::string_view name, std::string_view contents);


    enum
    {
        DEB_TRACE = 1,
        DEB_PASS = 2
    };

    void debugflags(uint32_t flags);

private:
    template <typename T>
    T& sym(std::string const& s) {
        return syms.at<T>(s);
    }

    std::any& sym(std::string& s) {
        return syms[s];
    }

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
    };

    struct Undefined
    {
        std::string name;
        std::pair<size_t, size_t> line_info;
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

    void setUndefined(std::string const& sym, SVWrap const& sv);
    void setUndefined(std::string const& sym);

    auto save() { return std::tuple(macros, syms, undefined, lastLabel); }

    template <class T>
    void restore(T const& t)
    {
        std::tie(macros, syms, undefined, lastLabel) = t;
    }

    void trace(SVWrap const& sv) const;

    bool doTrace = false;

    utils::path currentPath;
    std::unordered_map<std::string, std::string> includes;
    std::shared_ptr<Machine> mach;
    std::unordered_map<std::string_view, Macro> macros;
    std::unordered_map<std::string_view, Macro> definitions;
    Symbols syms;
    std::vector<Undefined> undefined;
    std::string_view lastLabel;
    bool finalPass{false};
    int passNo{0};

    std::string fileName;

    Parser parser;

    int labelNum = 0;
    int inMacro = 0;

    std::any parseResult;

    Scripting scripting;
};
