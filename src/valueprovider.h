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


#if 0  // unused
class GenericValueProvider {
protected:
    ValueType val;

public:
    const std::string_view __owner_classname__;

    GenericValueProvider() = delete;
    explicit GenericValueProvider(std::string const& owner_classname)
        : __owner_classname__(persist(owner_classname))
    {}
    GenericValueProvider(GenericValueProvider const& other)
        : val(other.val)
    {}
    GenericValueProvider(GenericValueProvider const&& other)
        : val(std::move(other.val))
    {}
    GenericValueProvider& operator=(GenericValueProvider const& other) {
        val = other.val;
        return *this;
    }
    GenericValueProvider& operator=(GenericValueProvider const&& other) {
        val = std::move(other.val);
        return *this;
    }
    virtual ~GenericValueProvider() = default;

    inline ValueType const& getAny() const { return val; }
    inline ValueType& getAny() { return val; }

    inline bool hasValue() const noexcept { return val.has_value(); }

    inline void resetValue() { val.reset(); }
};


template <typename T>
class ValueProvider : public GenericValueProvider {
public:
    ValueProvider() = delete;
    explicit ValueProvider(std::string& owner_classname) : GenericValueProvider(owner_classname) {}
    explicit ValueProvider(T const& v, std::string const& owner_classname)
        : GenericValueProvider(owner_classname)
    {
        this->val = v;
    }

    inline constexpr auto type() const {
        return typeid(T);
    }

    inline T getValue() const {
        auto const& val = this->val;
        if (val.has_value()) {
            return std::any_cast<T>(val);
        }
        throw std::runtime_error(fmt::format("Invalid conversion to type {} (\"{}\" is 'None')", typeid(T).name(), this->__owner_classname__));
    }

    inline void setValue(T const& newVal) {
        auto& val = this->val;
        if (val.has_value() && std::any_cast<T>(val) != newVal) {
            LOGD(fmt::format("Changing {} value from {} to {}\n", this->__owner_classname__, std::any_cast<T>(val), newVal));
            // throw?
        }
        val = newVal;
    }
};
#endif


class MultiValueProvider
{
protected:
    ValueMap vals;

public:
    using Map = std::unordered_map<std::string, MultiValueProvider*>;

    const std::string_view __owner_classname__;

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
    inline T getValue(std::string_view key)
    {
        // NOTE: prevent (accidental) nested std::optional's (as good as possible)
        // TODO: std::optional is a class template (requires our own Optional implementation)
        static_assert(false == std::is_same_v<T, std::optional<T>>);

        // TODO: std::variant is a class template (requires our own Variant implementation for RTTI)
        static_assert(false == std::is_same_v<T, AsmValue>);

        std::string s{key};
        auto const& val = vals[s];
        if (val.has_value()) {
            return std::any_cast<T>(val);
        }
        throw std::runtime_error(fmt::format("Invalid conversion to type {} (\"{}\" is 'None')", typeid(T).name(), this->__owner_classname__));
    }
};

#endif