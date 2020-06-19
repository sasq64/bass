#pragma once

#include <any>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Symbols is an unordered_map of string to any, with some extra
// functionality such as handling of recursive Symbols
class Symbols
{
public:
    using Str = std::string;
    using Any = std::any;
    using Map = std::unordered_map<Str, Any>;

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
    void forAll(FN const& fn, std::string const& name, Symbols const* current) const
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
            if(s == nullptr) {
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

private:
    Map data;
};


// SymbolTable for use in DSL. Remembers undefined references
// and value changes. Handles dot notation.
struct SymbolTable
{
    Symbols syms;

    template <typename T>
    void set(std::string_view name, T const& val)
    {
        auto was = syms.set(name, val);
    }

    void set(std::string_view name, std::any const& val);

    template <typename T>
    void set(std::vector<std::string_view> names, T const& v);
    void set(std::vector<std::string_view> names, std::any const& v)
    {
        Symbols* s = &syms;
        for (size_t i = 0; i < names.size() - 1; i++) {
            auto const& name = names[i];
            s = &s->at<Symbols>(name);
        }
        auto was = s->set(std::string(names.back()), v);
    }


    // Get a list of symbols that was undefined at first use
    std::vector<std::string_view> was_undefined();


};
