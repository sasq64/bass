#include "script.h"
#include "defines.h"

#include <coreutils/log.h>
#include <cstdio>
#include <sol/sol.hpp>
#include <string>

sol::state lua;

Scripting::Scripting()
{
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string,
                       sol::lib::math, sol::lib::table, sol::lib::debug);
}

void Scripting::load(utils::path const& p)
{
    lua.do_file(p.string());
}

void Scripting::add(std::string_view const& code)
{
    lua.script(code);
}

bool Scripting::hasFunction(std::string_view const& name)
{
    sol::protected_function test = lua[name];
    return test.valid();
}

std::any Scripting::call(std::string_view const& name,
                         std::vector<std::any> const& args)
{
    std::vector<sol::object> objs;
    sol::protected_function test = lua[name];

    for (auto const& a : args) {
        sol::object o;
        if (auto* an = std::any_cast<Number>(&a)) {
            o = sol::make_object(lua, *an);
        } else if (auto* as = std::any_cast<std::string_view>(&a)) {
            o = sol::make_object(lua, *an);
        }
        if (auto* av = std::any_cast<std::vector<uint8_t>>(&a)) {
            o = sol::make_object(lua, *av);
        }
        objs.push_back(o);
    }
    sol::protected_function_result fres = test.call(sol::as_args(objs));
    if (!fres.valid()) {
        sol::error err = fres;
        std::string what = err.what();
        throw script_error(what);
    }
    sol::object res = fres;
    if (res.is<double>()) {
        return std::any(res.as<double>());
    } else if (res.is<std::vector<uint8_t>>()) {
        return std::any(res.as<std::vector<uint8_t>>());
    }
    return std::any();
}
