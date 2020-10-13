#pragma once

#include <coreutils/path.h>

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace peg {
class parser;
struct SemanticValues;
} // namespace peg

struct SVWrap
{
    peg::SemanticValues const& sv;

    explicit SVWrap(peg::SemanticValues const& s);

    mutable std::string tok;
    mutable bool tokSet{false};

    const char* c_str() const;
    std::any operator[](size_t i) const;
    std::string const& token() const;
    std::string_view token_view() const;
    size_t size() const;
    const std::string& name() const;

    std::pair<size_t, size_t> line_info() const;

    std::type_info const& type(size_t i) const;

    template <typename T>
    std::vector<T> transform() const;

    int line() const { return static_cast<int>(line_info().first); }

    template <typename T>
    T to(size_t i) const
    {
        return std::any_cast<T>(operator[](i));
    }
};

template <>
std::vector<std::string> SVWrap::transform() const;
template <>
std::vector<std::string_view> SVWrap::transform() const;
template <>
std::vector<double> SVWrap::transform() const;
template <>
std::vector<std::any> SVWrap::transform() const;

enum class ErrLevel
{
    Warning,
    Error
};

struct Error
{
    Error(size_t line_ = 0, size_t column_ = 0,
          std::string const& message_ = "", ErrLevel level_ = ErrLevel::Error)
        : line(line_), column(column_), message(message_), level(level_)
    {}
    size_t line = 0;
    size_t column = 0;
    std::string message;
    std::string file;
    ErrLevel level{ErrLevel::Error};

    explicit operator bool() const { return line == 0; }
};

struct ParserWrapper
{

    Error currentError;

    std::unique_ptr<peg::parser> p;

    ~ParserWrapper();

    explicit ParserWrapper(const char* grammar);

    void packrat() const;
    void action(const char* name,
                std::function<std::any(SVWrap const&)> const& fn);

    void
    enter(const char* name,
          std::function<void(const char*, size_t, std::any&)> const&) const;
    void leave(const char* name,
               std::function<void(const char*, size_t, size_t, std::any&,
                                  std::any&)> const&) const;
    struct ActionSetter
    {
        const char* action;
        ParserWrapper* pw;

        template <typename FN>
        ActionSetter& operator=(FN const& fn)
        {
            pw->action(action, fn);
            return *this;
        }

        void enter(
            std::function<void(const char*, size_t, std::any&)> const& fn) const
        {
            pw->enter(action, fn);
        }
        void leave(std::function<void(const char*, size_t, size_t, std::any&,
                                      std::any&)> const& fn) const
        {
            pw->leave(action, fn);
        }
    };

    ActionSetter operator[](const char* action)
    {
        return ActionSetter{action, this};
    }

    Error parse(std::string_view source, const char* file, size_t line);
    Error parse(std::string_view source, size_t line);
    Error parse(std::string_view source, std::string const& file);
};

class parse_error : public std::exception
{
public:
    explicit parse_error(std::string m = "Parse error") : msg(std::move(m)) {}
    //    explicit parse_error(size_t l, std::string m = "Parse error")
    //        : line(l), msg(std::move(m))
    //    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    //   size_t line = 0;
    std::string msg;
};

/* class syntax_error : public std::exception */
/* { */
/* public: */
/*     explicit syntax_error(std::string m = "Syntax error") : msg(std::move(m))
 * {} */
/*     const char* what() const noexcept override { return msg.c_str(); } */

/*     std::string fileName; */
/*     std::pair<size_t, size_t> line_info; */

/* private: */
/*     std::string msg; */
/* }; */
