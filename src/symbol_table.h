#pragma once

#include "defines.h"

#include <coreutils/log.h>
#include <coreutils/text.h>

#include <fmt/format.h>

#include <any>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cassert>

using namespace std::string_literals;

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

struct Symbol
{
    std::any value;

    // This symbol is constant and may only be set once.
    bool final{false};
};

// Marks the symbol as a root of an `AnyMap`
struct Root
{};

template <typename NUM = double>
struct SymbolTable
{

    struct string_hash
    {
        using hash_type = std::hash<std::string_view>;
        using is_transparent = void;

        std::size_t operator()(const char* str) const
        {
            return hash_type{}(str);
        }
        std::size_t operator()(std::string_view str) const
        {
            return hash_type{}(str);
        }
        std::size_t operator()(std::string const& str) const
        {
            return hash_type{}(str);
        }
    };

private:
    std::unordered_map<std::string, Symbol, string_hash, std::equal_to<>> syms;
    // Symbols that was accessed _and_ did not exist in syms this pass
    std::unordered_set<std::string, string_hash, std::equal_to<>> undefined;
    // Symbols accessed this pass
    std::unordered_set<std::string, string_hash, std::equal_to<>> accessed;
    bool undef_ok = true;

public:
    bool trace = false;

    // Only false during final pass
    void accept_undefined(bool ok) { undef_ok = ok; }

    bool is_defined(std::string_view name) const { return syms.contains(name); }

    bool is_redefinable(std::string_view name) const
    {
        auto const it = syms.find(name);
        return !(it != syms.end() && it->second.final);
    }

    void set_sym(std::string_view name, AnyMap const& symbols)
    {
        for (auto const& p : symbols) {
            auto key = fmt::format("{}.{}", name, p.first);

            if (p.second.type() == typeid(AnyMap{})) {
                set_sym(key, std::any_cast<AnyMap>(p.second));
            } else {
                set(key, p.second);
            }
        }
        set(name, std::any{Root{}});
    }

    void set_sym(std::string name, Symbol const& sym)
    {
        syms[name] = sym;
    }

    void set_sym(std::string_view name, Symbol const& sym)
    {
        syms[std::string(name)] = sym;
    }


    std::optional<Symbol> get_sym(std::string_view name) const
    {
        auto it = syms.find(name);
        return it != syms.end() ? std::optional(it->second) : std::nullopt;
    }

    void set(const char* name, std::any const& val)
    {
        set((std::string const&)name, val);
    }

    void set(std::string_view name, std::any const& val)
    {
        set(std::string{name}, val);
    }

    void set(std::string const& name, std::any const& val)
    {
        if (!is_redefinable(name)) {
            throw sym_error(fmt::format("Symbol '{}' already defined", name));
        }
        if (val.type() == typeid(AnyMap)) {
            set_sym(name, std::any_cast<AnyMap>(val));
        } else if (val.type() == typeid(NUM)) {
            set(name, std::any_cast<NUM>(val));
        } else {
            auto s = std::string(name);
            if (trace && undefined.find(s) != undefined.end()) {
                fmt::print("Defined {}\n", s);
            }
            syms[s].value = val;
        }
    }

    template <typename T>
    void set(const char* name, T const& val)
    {
        set(static_cast<std::string const&>(name), val);
    }

    template <typename T>
    void set(std::string const& name, T const& val)
    {
        if (!is_redefinable(name)) {
            throw sym_error(fmt::format("Symbol '{}' already defined", name));
        }
        if constexpr (std::is_same_v<T, AnyMap>) {
            set_sym(name, static_cast<AnyMap>(val));
            return;
        } else {
            if (accessed.contains(name)) {
                if (trace) {
                    fmt::print("{} has been accessed\n", name);
                }
                auto it = syms.find(name);
                if (it != syms.end()) {
                    auto const& t = std::any_cast<T>(it->second.value);
                    if (t != val) {
                        if (trace) {
                            if constexpr (std::is_arithmetic_v<T>) {
                                fmt::print(
                                    "Redefined {} from {} "
                                    "to {}\n",
                                    name, std::any_cast<NUM>(it->second.value),
                                    val);
                            } else {
                                fmt::print("Redefined {} \n", name);
                            }
                        }
                        undefined.insert(name);
                    }
                } else {
                    if (trace) {
                        if constexpr (std::is_arithmetic_v<T>) {
                            fmt::print("Defined {}={}\n", name, val);
                        }
                    }
                }
            }

            if constexpr (std::is_arithmetic_v<T>) {
                syms[name].value = std::any((NUM)val);
            } else {
                syms[name].value = std::any(val);
            }
        }
    }

    void set_final(std::string_view name)
    {
        syms[std::string{name}].final = true;
    }

    // Return a map containing all symbols beginning with
    // name.
    AnyMap collect(std::string_view name) const
    {
        AnyMap s;
        for (auto const& p : syms) {
            auto dot = p.first.find('.');
            if (dot != std::string::npos) {
                auto prefix = p.first.substr(0, dot);
                if (prefix == name) {
                    // rest = one.x
                    auto rest = p.first.substr(dot + 1);
                    // Can never be undefined
                    s[rest] = syms.at(p.first).value;
                }
            }
        }
        return s;
    }

    template <typename T = std::any>
    T& get(std::string_view name)
    {
        static std::any temp;
        static T empty;
        static std::any zero(Number{});
        static AnyMap anyMap;
        accessed.insert(std::string(name));

        if constexpr (std::is_same_v<T, AnyMap>) {
            anyMap = collect(name);
            return anyMap;
        }

        auto it = syms.find(name);

        if (it == syms.end()) {
            // Handle symbol undefined

            if (!undef_ok) {
                throw sym_error(fmt::format("Undefined symbol '{}'", name));
            }
            LOGD("%s is undefined", name);
            if (trace) {
                fmt::print("Access undefined '{}'\n", name);
            }
            undefined.insert(std::string{name});
            if constexpr (std::is_same_v<T, std::any>) {
                LOGD(fmt::format("Returning zero value for undefined label '{}'\n", name));
                return zero;
            }
            LOGD("Returning default (%s)", typeid(T{}).name());
            return empty;
        }

        if (it->second.value.type() == typeid(Root)) {
            // We found a map
            if constexpr (std::is_same_v<T, std::any>) {
                anyMap = collect(name);
                temp = std::any{anyMap};
                return temp;
            } else {
                throw sym_error(fmt::format("Can not read map '{}'", name));
            }
        }
        if (it->second.value.type() == typeid(AnyMap)) {
            LOGE("MAP %s in table!!", name);
        }
        if constexpr (std::is_same_v<T, std::any>) {
            return it->second.value;
        }
        return *std::any_cast<T>(&it->second.value);
    }

    template <typename T>
    struct Accessor
    {
        SymbolTable& st;
        std::string_view name;

        Accessor(SymbolTable& st_, std::string_view name_)
            : st(st_), name(name_)
        {
        }

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
        return {*this, name};
    }

    template <typename FN>
    void forAll(FN const& fn) const
    {
        for (auto const& p : syms) {
            fn(p.first, p.second.value);
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
        return std::none_of(
            undefined.begin(), undefined.end(),
            [&](auto const& u) { return syms.find(u) == syms.end(); });
    }

    void erase(std::string_view name)
    {
        syms.erase(std::string(name));
        accessed.erase(std::string(name));
    }

    void erase_all(std::string_view name)
    {
        {
            auto it = syms.begin();
            while (it != syms.end()) {
                if (utils::startsWith(it->first, name)) {
                    it = syms.erase(it);
                } else {
                    it++;
                }
            }
        }
        {
            auto it = accessed.begin();
            while (it != accessed.end()) {
                if (utils::startsWith(*it, name)) {
                    it = accessed.erase(it);
                } else {
                    it++;
                }
            }
        }
    }

    bool done() const { return undefined.empty(); }

    auto const& get_undefined() const
    {
        return undefined;
    }

    void clear()
    {
        for (auto& s : syms) {
            s.second.final = false;
        }
        accessed.clear();
        undefined.clear();
    }
};
