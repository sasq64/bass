
#include "catch.hpp"

#include "symbol_table.h"

#include <string>

using namespace std::string_literals;

TEST_CASE("symboltable.basic", "[symbols]")
{
    using std::any_cast;

    Symbols s;

    REQUIRE(!s.is_defined("x"));

    s["hey"] = 3;
    REQUIRE(any_cast<int>(s["hey"]) == 3);

    auto& n = s.at<int>("hey");
    n = 5;
    REQUIRE(any_cast<int>(s["hey"]) == 5);

    REQUIRE(s.set("hey", 5) == Symbols::Was::Same);
    REQUIRE(any_cast<int>(s["hey"]) == 5);
    REQUIRE(s.set("hey", 2) == Symbols::Was::DifferentValue);
    REQUIRE(any_cast<int>(s["hey"]) == 2);
    REQUIRE(s.set("hey", "you"s) == Symbols::Was::DifferentType);
    REQUIRE(any_cast<std::string>(s["hey"]) == "you"s);
    REQUIRE(s.set("other", 9) == Symbols::Was::Undefined);


    s.at<Symbols>("x").at<Symbols>("y").at<int>("z") = 3;

    REQUIRE(s["x"].type() == typeid(Symbols));

    REQUIRE_THROWS(s.at<int>("x"));

    REQUIRE(s.at<Symbols>("x").at<Symbols>("y").at<int>("z") == 3);

    REQUIRE(s.get_as<int>({"x", "y", "z"}) == 3);

    REQUIRE(s.set({"x", "y", "p"}, 2) == Symbols::Was::Undefined);
    REQUIRE(s.set({"x", "y", "z"}, 9) == Symbols::Was::DifferentValue);

    REQUIRE_THROWS(s.set({"x", "y", "z", "w"}, 7));
    REQUIRE(s.set(std::vector{"x"s, "y"s}, 5) == Symbols::Was::DifferentType);

   /*
    * ex
    * a.z.p = 1 exists
    * a.r = 3 ; Undefined
    * a.z.p = 2 ; Different Value
    * a.z.p.y = 2 ; ? Different Type, Diff value, not allowed
    * */

}
