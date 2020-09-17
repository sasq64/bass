#pragma once

#include "defines.h"

#include <any>
#include <cstdint>
#include <vector>

template <int A, typename ARG>
ARG get_arg(std::vector<std::any> const& vec, std::false_type)
{
    static ARG empty{};
    return A < vec.size() ? std::any_cast<ARG>(vec[A]) : empty;
}

template <int A, typename ARG>
ARG get_arg(std::vector<std::any> const& vec, std::true_type)
{
    return static_cast<ARG>(A < vec.size() ? std::any_cast<double>(vec[A])
                                           : 0.0);
}

template <int A, typename ARG>
ARG get_arg(std::vector<std::any> const& vec)
{
    return get_arg<A, ARG>(vec, std::is_arithmetic<ARG>());
}

template <typename T>
std::any make_res(T&& v, std::true_type)
{
    return std::any(static_cast<double>(v));
}

template <typename T>
std::any make_res(T&& v, std::false_type)
{
    return std::any(v);
}

template <typename T>
std::any make_res(T&& v)
{
    return make_res(std::forward<T>(v), std::is_arithmetic<T>());
}

struct FunctionCaller
{
    virtual ~FunctionCaller() = default;
    virtual std::any call(std::vector<std::any> const&) const = 0;
};

template <typename... X>
struct FunctionCallerImpl;

template <class FX, class R>
struct FunctionCallerImpl<FX, R (FX::*)(std::vector<std::any> const&) const>
    : public FunctionCaller
{
    explicit FunctionCallerImpl(FX const& f) : fn(f) {}
    FX fn;

    std::any call(std::vector<std::any> const& args) const override
    {
        return make_res(fn(args));
    }
};

template <class FX, class R, class... ARGS>
struct FunctionCallerImpl<FX, R (FX::*)(ARGS...) const> : public FunctionCaller
{
    explicit FunctionCallerImpl(FX const& f) : fn(f) {}
    FX fn;

    template <size_t... A>
    std::any apply(std::vector<std::any> const& vec,
                   std::index_sequence<A...>) const
    {
        return make_res(fn(get_arg<A, ARGS>(vec)...));
    }

    std::any call(std::vector<std::any> const& args) const override
    {
        return apply(args, std::make_index_sequence<sizeof...(ARGS)>());
    }
};

// An `AnyCallable` holds a type erased function that can be called
// with an array of std::any, and that will extract the real types
// and call the stored function.
struct AnyCallable
{
    std::unique_ptr<FunctionCaller> fc;
    std::any operator()(std::vector<std::any> const& args) const
    {
        return fc->call(args);
    }

    AnyCallable() = default;

    template <typename FX>
    void init(FX const& f)
    {
        using FnType = decltype(&FX::operator());
        fc = std::make_unique<FunctionCallerImpl<FX, FnType>>(f);
    }

    template <typename FX>
    explicit AnyCallable(FX const& f)
    {
        init(f);
    }

    template <typename FX>
    AnyCallable& operator=(FX const& f)
    {
        init(f);
        return *this;
    }
};
