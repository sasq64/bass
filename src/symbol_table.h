#pragma once

#include <coreutils/log.h>

#include <any>
#include <fmt/format.h>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cassert>

using AnyMap = std::unordered_map<std::string, std::any>;

class sym_error : public std::exception
{
public:
    explicit sym_error(std::string m = "Symbol error") : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

// SymbolTable for use in DSL. Remembers undefined references
// and value changes. Handles dot notation.
// Setting specific type checks if value changed
//
struct SymbolTable
{
    std::unordered_map<std::string, std::any> syms;
    std::unordered_map<std::string, int> undefined;
    std::unordered_set<std::string> accessed;
    bool trace = false;

    bool is_defined(std::string_view name) const
    {
        return syms.find(std::string(name)) != syms.end();
    }

    void set_sym(std::string_view name, AnyMap const& symbols)
    {
        auto s = std::string(name);
        for (auto const& p : symbols) {
            if (p.second.type() == typeid(AnyMap{})) {
                set_sym(s + "." + p.first, std::any_cast<AnyMap>(p.second));
            } else {
                auto key = s + "." + p.first;
                set(key, p.second);
            }
        }
    }

    void set(std::string_view name, std::any const& val, int line = -1)
    {
        auto s = std::string(name);
        if (val.type() == typeid(AnyMap{})) {
            set(name, std::any_cast<AnyMap>(val), line);
        } else if (val.type() == typeid(double)) {
            set(name, std::any_cast<double>(val), line);
        } else {
            if (accessed.count(s) > 0) {
                throw sym_error(
                    fmt::format("Can not redefine generic any type {} ({})", s,
                                val.type().name()));
            }
            if (trace && undefined.find(s) != undefined.end()) {
                fmt::print("Defined {}\n", s);
            }
            syms[s] = val;
        }
    }

    template <typename T>
    void set(std::string_view name, T const& val, int line = -1)
    {
        (void)line;
        auto s = std::string(name);
        if constexpr (std::is_same_v<T, AnyMap>) {
            set_sym(s, static_cast<AnyMap>(val));
            return;
        } else {
            if (accessed.count(s) > 0) {
                LOGD("%s has been accessed", s);
                auto it = syms.find(s);
                if (it != syms.end()) {
                    if (std::any_cast<T>(it->second) != val) {
                        LOGD("Redefined '%s' in %d", s, line);
                        if (trace) {
                            if constexpr (std::is_arithmetic_v<T>) {
                                fmt::print("Redefined {} in line {} from {} "
                                           "to {}\n",
                                           s, line,
                                           std::any_cast<double>(it->second), val);
                            } else {
                                fmt::print("Redefined {} in line {}\n", s,
                                           line);
                            }
                        }
                        undefined.insert({s, line});
                    }
                } else {
                    if (trace) {
                        fmt::print("Defined {} in line {}\n", s, line);
                    }
                }
            }
            syms[s] = std::any(val);
        }
    }

    AnyMap collect(std::string const& name) const
    {
        AnyMap s;
        for (auto const& p : syms) {
            auto dot = p.first.find('.');
            if (dot != std::string::npos) {
                auto prefix = p.first.substr(0, dot);
                LOGD("Checking %s vs %s", prefix, name);
                if (prefix == name) {
                    // rest = one.x
                    auto rest = p.first.substr(dot + 1);
                    // Can never be undefined
                    s[rest] = syms.at(p.first);
                }
            }
        }
        return s;
    }

    template <typename T = std::any>
    T get(std::string_view name, int line = -1)
    {
        accessed.insert(std::string(name));
        if constexpr (std::is_same_v<T, AnyMap>) {
            auto s = std::string(name);
            auto res = collect(s);
            return res;
        }
        auto s = std::string(name);
        auto it = syms.find(s);
        if (it == syms.end()) {
            LOGD("%s is undefined", name);
            if (trace) {
                fmt::print("Access undefined '{}' in line {}\n", name, line);
            }
            undefined.insert({s, line});
            if constexpr (std::is_same_v<T, std::any>) {
                return std::any(static_cast<double>(0));
            }
            LOGD("Returning default (%s)", typeid(T{}).name());
            return T{};
        }
        if constexpr (std::is_same_v<T, std::any>) {
            return it->second;
        }
        return std::any_cast<T>(it->second);
    }

    template <typename T>
    struct Accessor
    {
        SymbolTable& st;
        std::string_view name;

        Accessor(SymbolTable& st_, std::string_view name_)
            : st(st_), name(name_)
        {}

        Accessor& operator=(T const& val)
        {
            st.set(name, val);
            return *this;
        }

        operator T const() const { return st.get<T>(name); } // NOLINT
    };

    template <typename T>
    Accessor<T> at(std::string_view name)
    {
        return Accessor<T>(*this, name);
    }

    Accessor<std::any> operator[](std::string_view name)
    {
        return Accessor<std::any>(*this, name);
    }

    template <typename FN>
    void forAll(FN const& fn) const
    {
        for (auto const& p : syms) {
            fn(p.first, p.second);
        }
    }

    // Remove all undefined that now exists
    void resolve()
    {
        for (auto const& [name, val] : syms) {
            undefined.erase(name);
        }
    }

    bool ok() const
    {
        for (auto const& u : undefined) {
            if (syms.find(u.first) == syms.end()) return false;
        }
        return true;
    }

    void erase(std::string_view name)
    {
        syms.erase(std::string(name));
        accessed.erase(std::string(name));
    }

    bool done() const { return undefined.empty(); }

    std::unordered_map<std::string, int> const& get_undefined() const
    {
        return undefined;
    }

    void clear()
    {
        accessed.clear();
        undefined.clear();
    }
};
