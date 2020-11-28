#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <type_traits>

namespace utils {

inline std::string const& toString(std::string const& s)
{
    return s;
}

inline std::string toString(char const* s)
{
    return std::string(s);
}

template <typename T>
inline std::string const& toString(const T& s)
{
    return std::to_string(s);
}

struct key_exception : public std::exception
{
    explicit key_exception(std::string const& msg) : message(msg) {}
    std::string message;

    const char* what() const noexcept override { return message.c_str(); }
};

// Get a key from a map and return default if not there
template <typename Container, typename Key, typename T>
inline auto map_get(Container const& c, Key&& key, T const& def)
{
    using namespace std::string_literals;
    auto it = c.find(std::forward<Key>(key));
    if (it == std::cend(c)) {
        return def;
    }
    return it->second;
}

// Get a key from a map and throw a readable exception if it is not found
template <typename Container, typename Key>
inline auto safe_get(Container const& c, Key&& key)
{
    using namespace std::string_literals;
    auto it = c.find(std::forward<Key>(key));
    if (it == std::cend(c)) {
        throw key_exception("Key "s + toString(std::forward<Key>(key)) +
                            " not found in map"s);
    }
    return it->second;
}

template <typename Container, typename Item>
inline void replace(Container& in, Item&& i0, Item&& i1)
{
    std::replace(std::begin(in), std::end(in), std::forward<Item>(i0),
                 std::forward<Item>(i1));
}

template <typename Item, typename Container>
inline auto replace_to(Container const& in, Item&& t0, Item&& t1)
{
    Container out = in;
    std::replace(std::begin(out), std::end(out), std::forward<Item>(t0),
                 std::forward<Item>(t1));
    return out;
}

template <typename Container, typename Op>
inline auto for_each(Container& in, Op&& op)
{
    std::for_each(std::begin(in), std::end(in), std::forward<Op>(op));
}

// Run 'op' for every element in 'in' and return the resulting container
template <typename Out = void, typename Container, typename Op>
inline auto transform_to(Container const& in, Op&& op)
{
    using Result =
        std::conditional_t<std::is_same<Out, void>::value, Container, Out>;
    Result out;
    std::transform(std::cbegin(in), std::cend(in),
                   std::inserter(out, std::begin(out)), std::forward<Op>(op));
    return out;
}

template <typename Out = void, typename Container, typename Op>
inline auto filter_to(Container const& in, Op&& op)
{
    using Result =
        std::conditional_t<std::is_same<Out, void>::value, Container, Out>;
    Result out;
    std::copy_if(std::cbegin(in), std::cend(in),
                 std::inserter(out, std::begin(out)), std::forward<Op>(op));
    return out;
}

template <typename Container, typename Comp>
inline void sort(Container& c, Comp&& comp)
{
    std::sort(std::begin(c), std::end(c), std::forward<Comp>(comp));
}

template <typename Container, typename Pred>
inline void stable_partition(Container& c, Pred&& pred)
{
    std::stable_partition(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename Container, typename Pred>
inline void partition(Container& c, Pred&& pred)
{
    std::partition(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename Container>
inline void sort(Container& c)
{
    std::sort(std::begin(c), std::end(c));
}

template <typename Out = void, typename Container>
inline auto copy_to(Container const& c)
{
    using Result =
        std::conditional_t<std::is_same<Out, void>::value, Container, Out>;
    Result out;
    std::copy(std::cbegin(c), std::cend(c),
              std::inserter(out, std::begin(out)));
    return out;
}

template <typename Container, typename Target>
inline void copy_back(Container const& c, Target& t)
{
    std::copy(std::cbegin(c), std::cend(c), std::back_inserter(t));
}

template <typename Container, typename Iterator>
inline void copy(Container const& c, Iterator&& iter)
{
    std::copy(std::cbegin(c), std::cend(c), std::forward<Iterator>(iter));
}

template <typename Container, typename Comp>
inline auto partition_point(Container& c, Comp&& comp)
{
    return std::partition_point(std::begin(c), std::end(c),
                                std::forward<Comp>(comp));
}

template <typename Container, typename Pred>
inline auto any_of(Container& c, Pred&& pred)
{
    return std::any_of(std::cbegin(c), std::cend(c), std::forward<Pred>(pred));
}

template <typename Container, typename Pred>
inline auto none_of(Container& c, Pred&& pred)
{
    return std::none_of(std::cbegin(c), std::cend(c), std::forward<Pred>(pred));
}

template <typename Container, typename Pred>
inline auto all_of(Container& c, Pred&& pred)
{
    return std::all_of(std::cbegin(c), std::cend(c), std::forward<Pred>(pred));
}

template <typename Container, typename VAL>
inline auto find(Container& c, VAL&& val)
{
    return std::find(std::begin(c), std::end(c), std::forward<VAL>(val));
}

template <typename Container, typename Pred>
inline auto find_if(Container& c, Pred&& pred)
{
    return std::find_if(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename Container, typename Pred>
inline auto find_ptr(Container& c, Pred&& pred)
{
    auto it =
        std::find_if(std::begin(c), std::end(c), std::forward<Pred>(pred));
    return it != std::end(c) ? &(*it) : nullptr;
}

template <typename Container, typename Pred>
inline auto find_opt(Container& c, Pred&& pred)
{
    auto it =
        std::find_if(std::begin(c), std::end(c), std::forward<Pred>(pred));
    return it != std::end(c) ? std::optional(*it) : std::nullopt;
}

template <typename Container, typename Pred>
inline auto remove_if(Container& c, Pred&& pred)
{
    return std::remove_if(std::begin(c), std::end(c), std::forward<Pred>(pred));
}

template <typename Container, typename T>
inline auto fill(Container& c, T&& v)
{
    return std::fill(std::begin(c), std::end(c), std::forward<T>(v));
}




} // namespace utils
