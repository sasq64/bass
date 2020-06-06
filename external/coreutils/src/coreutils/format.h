#pragma once

#include <cstring>
#include <string>
#include <vector>

#ifdef USE_FMT

#    include <fmt/format.h>
#    include <fmt/printf.h>

namespace utils {

template <class... A> std::string format(const std::string& fmt, A&&... args)
{
    return fmt::sprintf(fmt, std::forward<A>(args)...);
}

template <class... A> void print_fmt(const std::string& fmt, A&&... args)
{
    fmt::printf(fmt, std::forward<A>(args)...);
}

template <class... A> void println(const std::string& fmt = "", A&&... args)
{
    fmt::printf(fmt + "\n", std::forward<A>(args)...);
}

} // namespace utils

#else

namespace utils {

template <class... A> std::string format(const std::string& fmt, A&&... args)
{
    return fmt;//fmt::sprintf(fmt, std::forward<A>(args)...);
}

template <class... A> void print_fmt(const std::string& fmt, A&&... args)
{

    //fmt::printf(fmt, std::forward<A>(args)...);
}

template <class... A> void println(const std::string& fmt = "", A&&... args)
{
    //fmt::printf(fmt + "\n", std::forward<A>(args)...);
}

} // namespace utils

#endif

