#pragma once

#include "6502.h"
#include "symbol_table.h"

#include <any>
#include <coreutils/file.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <filesystem>
namespace fs = std::filesystem;

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

inline utils::File createFile(fs::path const& p)
{
    auto pp = p.parent_path();
    if (!pp.empty()) {
        fs::create_directories(pp);
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
    if (x == 0) throw dbz_error() // NOLINT

struct Num
{
    double d{};

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

    bool operator==(Num n) const { return d == n.d; }
    bool operator!=(Num n) const { return d != n.d; }
    bool operator<(Num n) const { return d < n.d; }
    bool operator>(Num n) const { return d > n.d; }
    bool operator<=(Num n) const { return d <= n.d; }
    bool operator>=(Num n) const { return d >= n.d; }

    int64_t i() const { return static_cast<int64_t>(d); }
    bool b() const { return static_cast<bool>(d); }
    explicit operator int64_t() const { return i(); }
    explicit operator double() const { return d; }
    explicit operator bool() const { return d != 0; }
};

inline std::string_view operator+(std::string_view sv, Num n)
{
    return persist(std::string(sv) + std::to_string(n.i()));
}

inline std::string_view operator+(std::string_view sv,
                                  std::string_view n)
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
    Instruction(std::string_view op, sixfive::Mode m, double v)
        : opcode(op), mode(m), val(static_cast<int32_t>(v))
    {}
    std::string_view opcode;
    sixfive::Mode mode;
    int32_t val;
};

inline std::string getHomeDir()
{
    std::string homeDir;
#if _WIN32
    char* userProfile = getenv("USERPROFILE");
    if (userProfile == nullptr) {

        char* homeDrive = getenv("HOMEDRIVE");
        char* homePath = getenv("HOMEPATH");

        if (homeDrive == nullptr || homePath == nullptr) {
            fprintf(stderr, "Could not get home directory");
            return "";
        }

        homeDir = std::string(homeDrive) + homePath;
    } else
        homeDir = std::string(userProfile);
#else
    homeDir = std::string(getenv("HOME"));
#endif
    return homeDir;
}

std::string any_to_string(std::any const& val);

inline void printArg(std::any const& arg)
{
    if (auto const* l = std::any_cast<Number>(&arg)) {
        if (*l == trunc(*l)) {
            fmt::print("${:x}", static_cast<int32_t>(*l));
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

struct RegState
{
    std::array<unsigned, 6> regs{0, 0, 0, 0, 0, 0};
    std::unordered_map<uint16_t, uint8_t> ram;
};

