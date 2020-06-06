#pragma once

#include <algorithm>
#include <string>
#include <type_traits>

using namespace std::string_literals;

namespace utils {

struct key_exception : public std::exception
{
    explicit key_exception(std::string const& msg) : message(msg) {}
    std::string message;

    const char* what() const noexcept override { return message.c_str(); }
};

template <typename C, typename KEY> auto safe_get(C const& c, KEY&& key)
{
    auto it = c.find(std::forward<KEY>(key));
    if (it == std::cend(c))
        throw key_exception("Key "s + key + " not found in map"s);
    return it->second;
}

template <typename CONTAINER, typename T>
void replace(CONTAINER& in, T&& t0, T&& t1)
{
    std::replace(std::begin(in), std::end(in), std::forward<T>(t0), std::forward<T>(t1));
}

template <typename T, typename CONTAINER>
auto replace_to(CONTAINER const& in, T&& t0, T&& t1)
{
    CONTAINER out = in;
    std::replace(std::begin(out), std::end(out), std::forward<T>(t0), std::forward<T>(t1));
    return out;
}

template <typename CONTAINER, typename OP>
auto for_each(CONTAINER const& in, OP&& op)
{
    std::for_each(std::cbegin(in), std::cend(in), std::forward<OP>(op));
}


// Run 'op' for every element in 'in' and return the resulting container
template <typename OUT = void, typename CONTAINER, typename OP>
auto transform_to(CONTAINER const& in, OP&& op)
{
    using RESULT =
        std::conditional_t<std::is_same<OUT, void>::value, CONTAINER, OUT>;
    RESULT out;
    std::transform(std::cbegin(in), std::cend(in),
                   std::inserter(out, std::begin(out)), std::forward<OP>(op));
    return out;
}

template <typename OUT = void, typename CONTAINER, typename OP>
auto filter_to(CONTAINER const& in, OP&& op)
{
    using RESULT =
        std::conditional_t<std::is_same<OUT, void>::value, CONTAINER, OUT>;
    RESULT out;
    std::copy_if(std::cbegin(in), std::cend(in),
                 std::inserter(out, std::begin(out)), std::forward<OP>(op));
    return out;
}

template <typename C, typename COMP> void sort(C& c, COMP&& comp)
{
    std::sort(std::begin(c), std::end(c), std::forward<COMP>(comp));
}

template <typename C> void sort(C& c)
{
    std::sort(std::begin(c), std::end(c));
}

template <typename C, typename OUT> auto copy_to(C const& c)
{
    using RESULT = std::conditional_t<std::is_same<OUT, void>::value, C, OUT>;
    RESULT out;
    std::copy(std::cbegin(c), std::cend(c),
              std::inserter(out, std::begin(out)));
    return out;
}

template <typename C, typename TARGET> void copy(C const& c, TARGET& t)
{
    std::copy(std::cbegin(c), std::cend(c), std::back_inserter(t));
}

template <typename C, typename COMP> auto partition_point(C& c, COMP&& comp)
{
    return std::partition_point(std::begin(c), std::end(c),
                                std::forward<COMP>(comp));
}

template <typename C, typename PRED> auto any_of(C& c, PRED&& pred)
{
    return std::any_of(std::cbegin(c), std::cend(c), std::forward<PRED>(pred));
}

template <typename C, typename PRED> auto none_of(C& c, PRED&& pred)
{
    return std::none_of(std::cbegin(c), std::cend(c), std::forward<PRED>(pred));
}

template <typename C, typename PRED> auto all_of(C& c, PRED&& pred)
{
    return std::all_of(std::cbegin(c), std::cend(c), std::forward<PRED>(pred));
}

template <typename C, typename PRED> auto find_if(C& c, PRED&& pred)
{
    return std::find_if(std::begin(c), std::end(c), std::forward<PRED>(pred));
}

template <typename C, typename PRED> auto find_ptr(C& c, PRED&& pred)
{
    auto it = std::find_if(std::begin(c), std::end(c), std::forward<PRED>(pred));
    return it != std::end(c) ? &(*it) : nullptr;
}

} // namespace utils
