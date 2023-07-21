#pragma once

#ifdef USE_BASS_VALUEPROVIDER

#include "coreutils/log.h"
#include "defines.h"

#include <optional>
#include <string>
#include <vector>


using ValueType = std::any;
using ValueMap = std::unordered_map<std::string, ValueType>;
#if 0
// NOTE: compatible to SymbolTable
using AnyMap = ValueMap;
#endif

#if 1
// TODO: Workaround until https://github.com/sasq64/bass/pull/42 is merged
#include <variant>
using AsmValue = std::variant<Number, std::string_view, std::vector<uint8_t>, std::vector<Number>>;
#endif

class MultiValueProvider
{
protected:
    ValueMap vals;
    std::string_view __owner_classname__;

public:
    using Map = std::unordered_map<std::string, MultiValueProvider*>;

    MultiValueProvider() = delete;
    explicit MultiValueProvider(std::string const& owner_classname);
    virtual ~MultiValueProvider();

    void setupValues(std::vector<std::string> const& identifiers);
    void setupRuntimeValue(std::string const& identifier);

    inline bool hasValue(std::string_view identifier) const
    {
        std::string s{identifier};
        auto it = vals.find(s);
        if (it == vals.end()) {
            return false;
        }
        return it->second.has_value();
    }

    inline bool hasAllValues() const
    {
        for (auto const& item : vals) {
            if (!item.second.has_value()) {
                return false;
            }
        }

        // all values are defined
        return true;
    }

    template <typename T>
    inline void setValue(std::string_view key, std::optional<T> const& opt_val)
    {
        // NOTE: prevent (accidental) nested std::optional's (as good as possible)
        // TODO: std::optional is a class template (requires our own Optional implementation)
        static_assert(false == std::is_same_v<T, std::optional<T>>);

        // TODO: std::variant is a class template (requires our own Variant implementation for RTTI)
        static_assert(false == std::is_same_v<T, AsmValue>);

        std::string s{key};
        if (!vals.contains(s)) {
            throw std::runtime_error(fmt::format("{} does not register value identifier '{}'", __owner_classname__, key));
        }
        if (opt_val.has_value()) {
            vals[s] = opt_val.value();
        } else if (vals.contains(s)) {
            LOGD(fmt::format("Invalidating value {}", key));
            vals[s].reset();
        }
    }

    template <typename T>
    inline T getValue(std::string_view key) const
    {
        // NOTE: prevent (accidental) nested std::optional's (as good as possible)
        // TODO: std::optional is a class template (requires our own Optional implementation)
        static_assert(false == std::is_same_v<T, std::optional<T>>);

        // TODO: std::variant is a class template (requires our own Variant implementation for RTTI)
        static_assert(false == std::is_same_v<T, AsmValue>);

        std::string s{key};
        auto it = vals.find(s);
        if (it == vals.end()) {
            throw std::runtime_error(fmt::format("{} does not register value identifier '{}'", __owner_classname__, key));
        }
        auto const& val = it->second;
        if (val.has_value()) {
            return std::any_cast<T>(val);
        }
        throw std::runtime_error(fmt::format("Invalid conversion to type {} (\"{}\" is 'None')", typeid(T).name(), this->__owner_classname__));
    }

    template <typename T>
    inline std::optional<T> getOptional(std::string_view key) const
    {
        // NOTE: prevent (accidental) nested std::optional's (as good as possible)
        // TODO: std::optional is a class template (requires our own Optional implementation)
        static_assert(false == std::is_same_v<T, std::optional<T>>);

        // TODO: std::variant is a class template (requires our own Variant implementation for RTTI)
        static_assert(false == std::is_same_v<T, AsmValue>);

        std::string s{key};
        auto it = vals.find(s);
        if (it == vals.end()) {
            throw std::runtime_error(fmt::format("{} does not register value identifier '{}'", __owner_classname__, key));
        }
        auto const& val = it->second;
        if (val.has_value()) {
            return std::any_cast<T>(val);
        }

        return {};
    }
};


template <typename T>
struct ValueSerializer final
{
    using GetFunc = std::function<T ()>;
    using SetFunc = std::function<void (T const&)>;
    //using SetOptFunc = std::function<void (std::optional<T> const&)>;

    std::string identifier;
    GetFunc getter;
    SetFunc setter;
    //SetOptFunc opt_setter;

    ValueSerializer() = default;
    ~ValueSerializer() = default;

    inline operator T () const { return getter(); }
    inline void operator=(T const& val) { setter(val); }
    //inline void operator=(std::optional<T> const& opt_val) { opt_setter(opt_val); }
    inline bool operator==(T val) const { return getter() == val; }
    //inline bool operator<(ValueSerializer<T> const& other) const { return getter() < other.getter(); }
    //inline bool operator==(std::optional<T> const& val) { return ; }
    inline bool operator<(T const& val) const { return getter() < val; }
    //inline bool operator<(ValueSerializer<T> const& other) const { return getter() < other.getter(); }
    inline bool operator<=(T const& val) const { return getter() <= val; }
    //inline bool operator<=(ValueSerializer<T> const& other) const { return getter() <= other.getter(); }
};


#endif
