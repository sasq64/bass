
#include "doctest.h"

#include "symbol_table.h"

#include <string>

using namespace std::string_literals;

/*
 * Any type should be settable
 * Default type T = double
 * Accessing an unknown returns T{} and flags that
 * symbol as "accessed".
 * Setting a value also set "defined".
 * The symbol table is "valid" if all accessed values
 * are also defined.
 * A value that was accessed while unknown should not
 * be allowed to be set a second time, since then it's
 * not clear what the value should have been while unknown.
 *
 * Symbols can be constant (final) or not.
 * Unknown symbols automatically become final.
 *
 * Dotted symbols
 *
 * If setting a symbol 'name' with a value that is a string map,
 * we iterate over the map and set each entry instead.
 *
 * If we read a symbol that is the prefix of several symbols followed
 * by a dot, we create a map and return instead.
 */
TEST_CASE("symbol_table.basic")
{
    SymbolTable<int> st;
    using std::any_cast;

    st.set("a", 3);

    int res = st.get<int>("a");
    REQUIRE(res == 3);

    REQUIRE(st.get_undefined().empty());
    REQUIRE(st.get<float>("not_here") == 0.0);
    REQUIRE(!st.get_undefined().empty());

    //REQUIRE(st.is_constant("not_here"));

    AnyMap s;
    s["x"] = 3;
    s["y"] = 2;
    st.set("pos", s);
    REQUIRE(st.get<int>("pos.x") == 3);
    REQUIRE(st.get<int>("pos.y") == 2);

    st.set("struct.x", 10);
    st.set("struct.y", 20);

    auto syms = st.get<AnyMap>("struct");
    REQUIRE(any_cast<int>(syms["x"]) == 10);
    REQUIRE(any_cast<int>(syms["y"]) == 20);

    AnyMap deep;
    deep["one"] = std::any(s);
    deep["two"] = std::any(syms);

    st.set("deep", deep);

    st.forAll([](auto const& s, auto const& v) { LOGI("%s", s); });

    REQUIRE(st.at<int>("deep.two.x") == 10);
    REQUIRE(st.get<int>("deep.one.y") == 2);

    REQUIRE_THROWS(st.set("a", "hey"s));

    //REQUIRE_THROWS(st.set("deep.two", syms));

    REQUIRE(!st.ok());

    REQUIRE(st.get_undefined().size() == 1);
    st.set("a", 9);
    REQUIRE(st.get_undefined().size() == 2);

    st.at<int>("not_here") = 5;

    REQUIRE(st.ok());
    REQUIRE(!st.done());

    st.clear();

    st.set("b", 4);
    st.set("c", "hey"s);

    REQUIRE(st.get<std::string>("c") == "hey");

    REQUIRE(st.done());
}
