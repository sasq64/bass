#pragma once

#include <coreutils/log.h>

#include <any>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cassert>
#define Assert assert

// Symbols is an unordered_map of string to any, with some extra
// functionality such as handling of recursive Symbols
class Symbols
{
public:
    using Str = std::string;
    using Any = std::any;
    using Map = std::unordered_map<Str, Any>;

    /*
     * collect all symbols starting with 'name'
     */
    Symbols collect(std::string const& name)
    {
        Symbols s;
        for (auto const& p : data) {
            auto dot = p.first.find('.');
            if (dot != std::string::npos) {
                auto prefix = p.first.substr(0, dot);
                LOGD("Checking %s vs %s", prefix, name);
                if (prefix == name) {
                    // rest = one.x
                    auto rest = p.first.substr(dot + 1);
                    s[rest] = p.second;
                }
            }
        }
        return s;
    }

    Any& operator[](Str const& name) { return data[name]; }

    template <typename T>
    T& at_(Str const& name)
    {
        auto const& aval = data[name];
        if (!aval.has_value()) {
            data[name] = T{};
        }
        T* t = std::any_cast<T>(&data[name]);
        if (t == nullptr) throw std::bad_any_cast();
        return *t;
    }

    template <typename T>
    T& at(std::string_view const& name)
    {
        return at_<T>(std::string(name));
    }

    void erase(std::string const& name) { data.erase(name); }

    bool is_defined(Str const& name) const
    {
        return data.find(name) != data.end();
    }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }

    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    std::any Undefined;

    // Return value or empty any if doesn't exist.
    // Record non-existing query
    std::any const& get(std::string const& name) const
    {
        if (is_defined(name)) {
            return data.at(name);
        }
        return Undefined;
    }

    template <typename FN>
    void forAll(FN const& fn, std::string const& name,
                Symbols const* current) const
    {
        for (auto const& p : *current) {
            std::string fullName =
                name.empty() ? p.first : name + "." + p.first;
            if (p.second.type() == typeid(Symbols)) {
                forAll(fn, fullName, std::any_cast<Symbols>(&p.second));
            } else {
                fn(fullName, p.second);
            }
        }
    }
    template <typename FN>
    void forAll(FN const& fn) const
    {
        forAll(fn, "", this);
    }

    std::any const& get(std::vector<std::string> const& names) const
    {
        Symbols const* s = this;
        for (size_t i = 0; i < names.size() - 1; i++) {
            auto const& name = names[i];
            if (!s->is_defined(name)) {
                return Undefined;
            }
            auto const& v = s->get(name);
            s = std::any_cast<Symbols>(&v);
            if (s == nullptr) {
                // This was not a symbol table
                return Undefined;
            }
        }
        return s->get(names.back());
    }

    template <typename T>
    T const& get_as(std::vector<std::string> const& names) const
    {
        auto const& v = get(names);
        return *std::any_cast<T>(&v);
    }

    enum class Was
    {
        Undefined,
        DifferentType,
        DifferentValue,
        Same,
        SameType
    };

    Was set(std::string const& name, std::any const& v)
    {
        if (!is_defined(name)) {
            data[name] = v;
            return Was::Undefined;
        }
        auto old = data[name];
        if (old.type() == v.type()) {
            data[name] = v;
            return Was::SameType;
        } else {
            data[name] = v;
            return Was::DifferentType;
        }
    }

    // Record if value was changed
    template <typename T>
    Was set(std::string const& name, T const& val)
    {
        auto v = std::any(val);
        if (!is_defined(name)) {
            data[name] = v;
            return Was::Undefined;
        }
        auto old = data[name];
        if (old.type() == v.type()) {
            auto& target = at<T>(name);
            if (target != val) {
                target = val;
                return Was::DifferentValue;
            } else {
                return Was::Same;
            }
        } else {
            data[name] = v;
            return Was::DifferentType;
        }
    }

    // Record if value was changed
    template <typename T>
    Was set(std::vector<std::string> const& names, T const& val)
    {
        Symbols* s = this;
        for (size_t i = 0; i < names.size() - 1; i++) {
            auto const& name = names[i];
            s = &s->at<Symbols>(name);
        }
        return s->set(names.back(), val);
    }

    Map const& getData() const { return data; }

private:
    Map data;
};

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

    bool is_defined(std::string_view name) const
    {
        return syms.find(std::string(name)) != syms.end();
    }

    void set_sym(std::string_view name, Symbols const& symbols)
    {
        auto s = std::string(name);
        for (auto const& p : symbols.getData()) {
            LOGD("Type %s", p.second.type().name());
            if (p.second.type() == typeid(Symbols{})) {
                LOGD("Recurse");
                set_sym(s + "." + p.first, std::any_cast<Symbols>(p.second));
            } else {
                auto key = s + "." + p.first;
                LOGI("Set %s", key);
                set(key, p.second);
            }
        }
    }

    template <typename T>
    void set(std::string_view name, T const& val)
    {
        auto s = std::string(name);
        if constexpr (std::is_same_v<T, std::any>) {
            if (val.type() == typeid(Symbols{})) {
                set_sym(s, std::any_cast<Symbols>(val));
                return;
            }
            if (accessed.count(s) > 0) {
                LOGD("%s has been accessed", s);
                auto it = syms.find(s);
                if (it != syms.end()) {
                    LOGD("%s vs %s", it->second.type().name(),
                         val.type().name());
                    if (it->second.type() != val.type()) {
                        throw sym_error("Can not change type");
                    }
                    auto a = std::any_cast<double>(it->second);
                    auto b = std::any_cast<double>(val);
                    if (a != b) {
                        LOGI("Redefined '%s' %f %f", s, a, b);
                        undefined.insert({s, -1});
                    }
                }
            }
            syms[s] = val;
        } else if constexpr (std::is_same_v<T, Symbols>) {
            set_sym(s, static_cast<Symbols>(val));
            return;
        } else {
            if (accessed.count(s) > 0) {
                LOGD("%s has been accessed", s);
                auto it = syms.find(s);
                if (it != syms.end()) {
                    LOGD("%s vs %s", it->second.type().name(),
                         typeid(val).name());
                    if (it->second.type() != typeid(val)) {
                        throw sym_error("Can not change type");
                    }
                    if (std::any_cast<T>(it->second) != val) {
                        LOGD("Redefined '%s'", s);
                        undefined.insert({s, -1});
                    }
                }
            }
            syms[s] = std::any(val);
        }
    }

    Symbols collect(std::string const& name) const
    {
        Symbols s;
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
        if constexpr (std::is_same_v<T, Symbols>) {
            auto s = std::string(name);
            auto res = collect(s);
            return res;
        }
        auto s = std::string(name);
        auto it = syms.find(s);
        if (it == syms.end()) {
            LOGI("%s is undefined", name);
            undefined.insert({s, line});
            if constexpr (std::is_same_v<T, std::any>) {
                return std::any((double)0);
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

        operator T const() const { return st.get<T>(name); }
    };

    template <typename T>
    Accessor<T> at(std::string_view name)
    {
        return Accessor<T>(*this, name);
    }

    /* Accessor<std::any> operator[](std::string_view name) */
    /* { */
    /*     return Accessor<std::any>(*this, name); */
    /* } */

    template <typename FN>
    void forAll(FN const& fn) const
    {
        for (auto const& p : syms) {
            fn(p.first, p.second);
        }
    }

    bool ok()
    {
        for (auto const& u : undefined) {
            if (syms.find(u.first) == syms.end()) return false;
        }
        return true;
    }

    void erase(std::string_view name) { syms.erase(std::string(name)); }

    bool done() const { return undefined.empty(); }

    void clear()
    {
        accessed.clear();
        undefined.clear();
    }
};
