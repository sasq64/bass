#pragma once

#include "6502.h"
#include "symbol_table.h"

#include <coreutils/file.h>
#include <coreutils/path.h>
#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// using Number = double;

// Update string view so contents is stored persistently
inline std::string const& persist(std::string_view& sv)
{
    static std::unordered_set<std::string> persisted;
    auto const& res = *persisted.emplace(sv).first;
    sv = res;
    return res;
}

// Store string and return string_view to it
inline std::string_view persist(std::string const& s)
{
    std::string_view sv(s);
    persist(sv);
    return sv;
}

inline utils::File createFile(utils::path const& p)
{
    auto pp = p.parent_path();
    if(!pp.empty()) {
        utils::create_directories(pp);
    }
    return utils::File{p.string(), utils::File::Mode::Write};
}


class dbz_error : public std::exception
{
public:
    explicit dbz_error(std::string m = "Division by zero") : msg(std::move(m))
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

#define DBZ(x)                                                                 \
    if (x == 0) throw dbz_error()

struct Num
{
    double d;

    Num() = default;
    template <typename T>
    Num(T t_) : d(static_cast<double>(t_)) // NOLINT
    {}
    Num operator+(Num n) const { return d + n.d; }
    Num operator-(Num n) const { return d - n.d; }
    Num operator*(Num n) const { return d * n.d; }
    Num operator/(Num n) const
    {
        DBZ(n.d);
        return d / n.d;
    }

    Num operator|(Num n) const { return i() | n.i(); };
    Num operator&(Num n) const { return i() & n.i(); };
    Num operator^(Num n) const { return i() ^ n.i(); };
    Num operator>>(Num n) const { return i() >> n.i(); };
    Num operator<<(Num n) const { return i() << n.i(); };
    Num operator%(Num n) const
    {
        DBZ(n.i());
        return i() % n.i();
    };

    Num operator&&(Num n) const { return b() && n.b(); }
    Num operator||(Num n) const { return b() || n.b(); }

    Num operator==(Num n) const { return d == n.d; }
    Num operator!=(Num n) const { return d != n.d; }
    Num operator<(Num n) const { return d < n.d; }
    Num operator>(Num n) const { return d > n.d; }
    Num operator<=(Num n) const { return d <= n.d; }
    Num operator>=(Num n) const { return d >= n.d; }

    int64_t i() const { return static_cast<int64_t>(d); }
    bool b() const { return static_cast<bool>(d); }
    explicit operator int64_t() const { return i(); }
    explicit operator double() const { return d; }
    explicit operator bool() const { return d != 0; }
};

inline std::string_view operator+(std::string_view const& sv, Num n)
{
    return persist(std::string(sv) + std::to_string(n.i()));
}

inline std::string_view operator+(std::string_view const& sv,
                                  std::string_view const& n)
{
    return persist(std::string(sv) + std::string(n));
}

using Number = double;

inline Num div(Num a, Num b)
{
    DBZ(b.i());
    return a.i() / b.i();
}

inline Num pow(Num a, Num b)
{
    return pow(a.d, b.d);
}

template <typename T>
inline T number(std::any const& v)
{
    return static_cast<T>(std::any_cast<Number>(v));
}

inline Number number(std::any const& v)
{
    return std::any_cast<Number>(v);
}

template <typename T>
inline Number num(T const& v)
{
    return static_cast<Number>(v);
}

template <typename T>
inline std::any any_num(T const& v)
{
    return std::any(static_cast<Number>(v));
}

class assert_error : public std::exception
{
public:
    explicit assert_error(std::string m = "Assert failed") : msg(std::move(m))
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

struct Instruction
{
    Instruction(std::string_view const& op, sixfive::Mode m, double v)
        : opcode(op), mode(m), val(static_cast<int32_t>(v))
    {}
    std::string_view opcode;
    sixfive::Mode mode;
    int32_t val;
};


inline void printArg(std::any const& arg)
{
    if (auto const* l = std::any_cast<Number>(&arg)) {
        if (*l == trunc(*l)) {
            fmt::print("{}", static_cast<int32_t>(*l));
        } else {
            fmt::print("{}", *l);
        }
    } else if (auto const* s = std::any_cast<std::string_view>(&arg)) {
        fmt::print("{}", *s);
    } else if (auto const* v = std::any_cast<std::vector<uint8_t>>(&arg)) {
        for (auto const& item : *v) {
            fmt::print("{:02x} ", item);
        }
    }
}
