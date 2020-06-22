
#include "catch.hpp"

#include "symbol_table.h"

#include <string>

using namespace std::string_literals;

TEST_CASE("symbol_table.basic", "[symbols]")
{
    SymbolTable st;

    st.set("a", 3);

    int res = st.get<int>("a");
    REQUIRE(res == 3);

    REQUIRE(st.undefined.empty());
    REQUIRE(st.get<float>("not_here") == 0.0);
    REQUIRE(!st.undefined.empty());

    Symbols s;
    s.set("x", 3);
    s.set("y", 2);
    st.set("pos", s);
    REQUIRE(st.get<int>("pos.x") == 3);
    REQUIRE(st.get<int>("pos.y") == 2);

    st.set("struct.x", 10);
    st.set("struct.y", 20);

    auto syms = st.get<Symbols>("struct");
    REQUIRE(syms.at<int>("x") == 10);
    REQUIRE(syms.at<int>("y") == 20);

    Symbols deep;
    deep.set("one", std::any(s));
    deep.set("two", std::any(syms));

    st.set("deep", deep);

    st.forAll([](auto const& s, auto const& v) { LOGI("%s", s); });

    REQUIRE(st.get<int>("deep.two.x") == 10);
    REQUIRE(st.get<int>("deep.one.y") == 2);

    REQUIRE_THROWS(st.set("a", "hey"s));


    REQUIRE_THROWS(st.set("deep.two", syms));

    REQUIRE(!st.ok());

    REQUIRE(st.undefined.size() == 1);
    st.set("a", 9);
    REQUIRE(st.undefined.size() == 2);

    st.set("not_here", 5);

    REQUIRE(st.ok());
    REQUIRE(!st.done());

    st.clear_undef();

    st.set("b", 4);
    st.set("c", "hey"s);

    REQUIRE(st.get<std::string>("c") == "hey");

    REQUIRE(st.done());

}

TEST_CASE("symbols.basic", "[symbols]")
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
