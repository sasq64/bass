#pragma once
#include "defines.h"
#include "parser.h"
#include <any>
#include <coreutils/utf8.h>

struct Value
{
    std::any val;

    std::u32string unicode;

    template <typename T>
    Value(T&& t)
    {
        set(std::forward<T>(t), std::is_arithmetic<T>());
    }

    std::type_info const& type() { return val.type(); }

    template <typename T>
    void set(T&& t, std::true_type)
    {
        val = static_cast<Number>(t);
    }

    template <typename T>
    void set(T&& t, std::false_type)
    {
        val = t;
    }

    Value(std::any a) : val(std::move(a)) {}

    bool is_callable() const { return val.type() == typeid(AstNode); }
    bool is_numeric() const { return val.type() == typeid(Number); }
    bool is_array() const
    {
        return val.type() == typeid(std::vector<uint8_t>) ||
               val.type() == typeid(std::vector<Number>) ||
               val.type() == typeid(std::string_view);
    }

    template <typename RET, typename... ARGS>
    RET operator()(ARGS... args)
    {}

    template <typename T>
    T const& get_num(std::true_type) const
    {
        static T t = static_cast<T>(std::any_cast<Number>(val));
        return t;
    }

    template <typename T>
    T const& get_num(std::false_type) const
    {
        return *std::any_cast<T>(&val);
    }

    template <typename T>
    T* getp() const
    {
        return std::any_cast<T>(&val);
    }

    Number operator[](size_t i) const
    {
        if (auto const* v8 = std::any_cast<std::vector<uint8_t>>(&val)) {
            return v8->at(i);
        }
        if (auto const* vn = std::any_cast<std::vector<Number>>(&val)) {
            return vn->at(i);
        }
        throw parse_error("Can not index non-array");
    }

    Value slice(size_t start, size_t end) const
    {
        if (auto const* v8 = std::any_cast<std::vector<uint8_t>>(&val)) {
            std::vector<uint8_t> nv(v8->begin() + start, v8->begin() + end);
            return {nv};
        }
        if (auto const* vn = std::any_cast<std::vector<Number>>(&val)) {
            std::vector<Number> nv(vn->begin() + start, vn->begin() + end);
            return {nv};
        }
        throw parse_error("Can not slice non-array");
    }

    template <typename FN>
    void for_each(FN&& fn) const
    {
        if (auto const* v8 = std::any_cast<std::vector<uint8_t>>(&val)) {
            std::for_each(v8->begin(), v8->end(), std::forward<FN>(fn));
        }
        if (auto const* vn = std::any_cast<std::vector<Number>>(&val)) {
            std::for_each(vn->begin(), vn->end(), std::forward<FN>(fn));
        }
        if (auto const* vs = std::any_cast<std::string_view>(&val)) {
            auto ustr = utils::utf8_decode(*vs);
            std::for_each(ustr.begin(), ustr.end(), std::forward<FN>(fn));
        }
        throw parse_error("Not an array");
    }

    bool operator==(Value const& v) const
    {
        if (val.type() != v.val.type()) return false;

        return true;
    }

    template <typename T>
    T const& get()
    {
        return get_num<T>(std::is_arithmetic<T>());
    }
};
