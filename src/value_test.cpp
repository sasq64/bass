#include "catch.hpp"
#include "value.h"


TEST_CASE("value.basic", "[value]")
{
    using V8 = std::vector<uint8_t>;
    Value v{3};

    REQUIRE(!v.is_callable());
    REQUIRE(v.is_numeric());

    REQUIRE(v.type() == typeid(Number));

    REQUIRE(v.get<uint8_t>() == 3);

    Value v2{std::vector<uint8_t>{1,2,3,4,5}};

    REQUIRE(v2[1] == 2); 
    auto slice = v2.slice(1, 4);
    REQUIRE(slice.get<V8>() == V8{2,3,4});

    size_t total = 0;
    slice.for_each([&](auto a) { total += a; });
    REQUIRE(total == 9);
}
