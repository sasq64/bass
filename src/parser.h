#pragma once

#include <any>
#include <coreutils/file.h>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

extern "C"
{
#include <sha512.h>
}

namespace peg {
class parser;
template <typename T>
struct AstBase;
} // namespace peg

struct BassNode;
using AstNode = std::shared_ptr<peg::AstBase<BassNode>>;

template <typename C>
inline std::string hex_encode(C const& c, int len = 0)
{
    static const char* hex = "0123456789abcdef";
    std::string result;
    for (uint8_t b : c) {
        result += hex[b >> 4];
        result += hex[b & 0xf];
        if (len-- == 0) {
            break;
        }
    }
    return result;
}

class parser_error : public std::exception
{
public:
    explicit parser_error(std::string m = "Parser error") : msg(std::move(m))
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

class SemanticValues
{
    std::shared_ptr<peg::AstBase<BassNode>> const& ast;

public:
    explicit SemanticValues(AstNode const&);
    ~SemanticValues() = default;
    size_t line() const { return line_info().first; }
    std::pair<size_t, size_t> line_info() const;
    std::any operator[](size_t i) const;
    std::string_view token_view() const;
    size_t size() const;
    std::string_view name() const;

    AstNode get_node() const { return ast; }
    std::string const& file_name() const;

    template <typename T>
    T to(size_t i) const
    {
        return std::any_cast<T>(operator[](i));
    }
};

using ActionFn = std::function<std::any(SemanticValues const&)>;

AstNode get_child(AstNode node, size_t i);

enum class ErrLevel
{
    Warning,
    Error
};

struct Error
{
    Error() = default;
    Error(size_t line_, size_t column_, std::string const& message_,
          ErrLevel level_ = ErrLevel::Error)
        : line(line_), column(column_), message(message_), level(level_)
    {}
    size_t line = 0;
    size_t column = 0;
    std::string message;
    std::string file;
    ErrLevel level{ErrLevel::Error};
};

class Parser
{
    Error currentError;
    bool tracing = false;
    std::unordered_map<std::string, std::function<bool(SemanticValues const&)>>
        preActions;
    std::unordered_map<std::string, ActionFn> postActions;
    std::string currentFile;
    std::string_view currentSource;
    std::vector<std::string_view> ruleNames;
    std::unordered_map<std::string_view, size_t> ruleMap;

    std::array<uint8_t, SHA512_DIGEST_LENGTH> grammarSHA{};

    std::unique_ptr<peg::parser> p;
    bool haveError{false};

    std::any callAction(SemanticValues& sv, ActionFn const& fn);

    bool useCache = true;

public:
    ~Parser();

    explicit Parser(const char* grammar);
    void setError(std::string const& what, std::string_view file, size_t line);

    void use_cache(bool on) { useCache = on; }

    void packrat() const;
    void before(const char* name,
                std::function<bool(SemanticValues const&)> const& fn);
    void after(const char* name,
               std::function<std::any(SemanticValues const&)> const& fn);

    void
    enter(const char* name,
          std::function<void(const char*, size_t, std::any&)> const&) const;
    void leave(const char* name,
               std::function<void(const char*, size_t, size_t, std::any&,
                                  std::any&)> const&) const;
    Error getError() const { return currentError; }

    AstNode parse(std::string_view source, std::string_view file);

    std::any evaluate(AstNode const& node);

    void doTrace(bool on) { tracing = on; };
    void saveAst(utils::File& f, const AstNode& root);
    AstNode loadAst(utils::File& f);
};

class parse_error : public std::exception
{
public:
    explicit parse_error(std::string m = "Parse error") : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};
