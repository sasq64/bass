#ifdef USE_BASS_VALUEPROVIDER

#include "doctest.h"
//#include "test_utils.h"
#include "valueprovider.h"


using namespace std::string_literals;

TEST_CASE("multivalueprovider.base")
{
    MultiValueProvider mvp("TestMultiValueProvider"s);

    mvp.setupValues({
        "valueprovider.example.value_1"s,
        "valueprovider.example.value_2"s,
    });

    REQUIRE_FALSE(mvp.hasAllValues());
}

TEST_CASE("multivalueprovider.fails_with_unregistered_value")
{
    MultiValueProvider mvp("TestMultiValueProvider"s);

    mvp.setupValues({
        "valueprovider.example.value_1"s,
        "valueprovider.example.value_2"s,
    });

    REQUIRE_THROWS(
        mvp.setValue<std::string>("nope"s, "i am an unregistered value"s)
        );
}

TEST_CASE("multivalueprovider.insert_runtime_value")
{
    MultiValueProvider mvp("TestMultiValueProvider"s);

    mvp.setupValues({
        "valueprovider.example.value_1"s,
        "valueprovider.example.value_2"s,
    });

    // simulates an "accessed/undefined" label during parsing
    std::string test_symbol_0{"symbol_0"};
    mvp.setupRuntimeValue(test_symbol_0);                   // None

    mvp.setValue<int32_t>(test_symbol_0, {});               // still None
    REQUIRE_FALSE(mvp.hasValue(test_symbol_0));
    REQUIRE_THROWS(mvp.getValue<int32_t>(test_symbol_0));

    mvp.setValue<int32_t>(test_symbol_0, 42);               // 42 (first assignement)
    REQUIRE(mvp.hasValue(test_symbol_0));
    REQUIRE_EQ(mvp.getValue<int32_t>(test_symbol_0), 42);

    // NOTE: currently re-assigning is allowed to ease transition
    // TODO: fail (meaning: "Symbol::finished == true")
    int32_t fourty_three = 43;
    mvp.setValue(test_symbol_0, std::optional{fourty_three});  // outputs "value runtime.value_0 changed 42 -> 43"
    REQUIRE_EQ(mvp.getValue<int32_t>(test_symbol_0), 43);
}

#endif
