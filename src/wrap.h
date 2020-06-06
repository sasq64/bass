#pragma once

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

    const char* c_str() const;
    std::any operator[](size_t i) const;
    std::string token() const;
    std::string_view token_view() const;
    size_t size() const;
    const std::string& name() const;

    std::pair<size_t, size_t> line_info() const;

    std::type_info const& type(size_t i) const;

    template <typename T>
    std::vector<T> transform() const;

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

enum class ErrLevel {Warning, Error};

struct Error
{
    size_t line;
    size_t column;
    std::string message;
    ErrLevel level{ErrLevel::Error};
};


struct ParserWrapper
{
    std::vector<Error> errors;


    std::unique_ptr<peg::parser> p;

    ~ParserWrapper();

    explicit ParserWrapper(std::string const& source);

    void action(const char* name,
                std::function<std::any(SVWrap const&)> const& fn) const;

    void enter(const char* name,
               std::function<void(const char*, size_t, std::any&)> const&);
    void leave(const char* name,
               std::function<void(const char*, size_t, size_t, std::any&,
                                  std::any&)> const&);
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

        void
        enter(std::function<void(const char*, size_t, std::any&)> const& fn)
        {
            pw->enter(action, fn);
        }
        void leave(std::function<void(const char*, size_t, size_t, std::any&,
                                      std::any&)> const& fn)
        {
            pw->leave(action, fn);
        }
    };

    ActionSetter operator[](const char* action)
    {
        return ActionSetter{action, this};
    }

    bool parse(std::string_view source, const char* name = nullptr) const;
    bool parse(std::string_view source, std::any& d, const char* file = nullptr) const;
};

class parse_error : public std::exception
{
public:
    explicit parse_error(std::string m = "Parse error") : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

/* class syntax_error : public std::exception */
/* { */
/* public: */
/*     explicit syntax_error(std::string m = "Syntax error") : msg(std::move(m)) {} */
/*     const char* what() const noexcept override { return msg.c_str(); } */

/*     std::string fileName; */
/*     std::pair<size_t, size_t> line_info; */

/* private: */
/*     std::string msg; */
/* }; */

