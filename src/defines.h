#pragma once

#include "enums.h"
#include "symbol_table.h"

//#include <sol/forward.hpp>

#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// using Number = double;

inline std::string const& persist(std::string_view& sv)
{
    static std::unordered_set<std::string> persisted;
    auto const& res = *persisted.emplace(sv).first;
    sv = res;
    return res;
}

struct Num
{
    double d;

    Num() = default;
    template <typename T>
    Num(T t_) : d(static_cast<double>(t_))
    {}
    Num operator+(Num n) const { return d + n.d; }
    Num operator-(Num n) const { return d - n.d; }
    Num operator*(Num n) const { return d * n.d; }
    Num operator/(Num n) const { return d / n.d; }

    Num operator|(Num n) const { return i() | n.i(); };
    Num operator&(Num n) const { return i() & n.i(); };
    Num operator^(Num n) const { return i() ^ n.i(); };
    Num operator>>(Num n) const { return i() >> n.i(); };
    Num operator<<(Num n) const { return i() << n.i(); };
    Num operator%(Num n) const { return i() % n.i(); };

    Num operator&&(Num n) const { return d && n.d; }
    Num operator||(Num n) const { return d || n.d; }

    Num operator==(Num n) const { return d == n.d; }
    Num operator!=(Num n) const { return d != n.d; }
    Num operator<(Num n) const { return d < n.d; }
    Num operator>(Num n) const { return d > n.d; }
    Num operator<=(Num n) const { return d <= n.d; }
    Num operator>=(Num n) const { return d >= n.d; }

    int64_t i() const { return static_cast<int64_t>(d); }
    operator int64_t() const { return i(); }
    operator double() const { return d; }
    operator bool() const { return d != 0; }
};

inline std::string_view operator+(std::string_view const& sv, Num n)
{
    auto s = std::string_view(std::string(sv) + std::to_string(n.i()));
    persist(s);
    return s;
}

inline std::string_view operator+(std::string_view const& sv,
                           std::string_view const& n)
{
    auto s = std::string_view(std::string(sv) + std::string(n));
    persist(s);
    return s;
}

using Number = double;

inline Num div(Num a, Num b)
{
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
    Instruction(std::string const& op, sixfive::AdressingMode m, double v)
        : opcode(op), mode(m), val((int32_t)v)
    {}
    std::string opcode;
    sixfive::AdressingMode mode;
    int32_t val;
};

Symbols loadSid(std::string_view const& name);
Symbols loadPng(std::string_view const& name);

enum
{
    NO_SUCH_OPCODE = -1,
    ILLEGAL_ADRESSING_MODE = -2
};
