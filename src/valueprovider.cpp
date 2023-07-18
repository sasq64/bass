#ifdef USE_BASS_VALUEPROVIDER

#include "valueprovider.h"


MultiValueProvider::Map bass_provided_values;


MultiValueProvider::MultiValueProvider(std::string const& owner_classname)
    : __owner_classname__(persist(owner_classname))
{}

MultiValueProvider::~MultiValueProvider()
{
    for (auto it : vals) {
        bass_provided_values.erase(it.first);
    }
}

void MultiValueProvider::setupValues(std::vector<std::string> const& identifiers) {
    for (auto s : identifiers) {
        auto const [it, inserted] = bass_provided_values.insert({s, this});
        auto const mvp = it->second;

        if (mvp != this) {
            //assert(inserted == false);
            throw std::runtime_error(
                fmt::format("{} cannot register identifier '{}' which is already registered by {})",
                            (*this).__owner_classname__,
                            it->first,
                            (*it->second).__owner_classname__)
                );
        }

        // initialize value (always a 'None' value)
        vals[s];
    }
}

void MultiValueProvider::setupRuntimeValue(std::string const& identifier)
{
    auto const [it, inserted] = bass_provided_values.insert({identifier, this});
    auto const mvp = it->second;

    if (inserted) {
        // initialize value (always a 'None' value)
        vals[identifier];
    } else {
        throw std::runtime_error(fmt::format("Symbol '{}' already defined", identifier));
    }
}

#endif
