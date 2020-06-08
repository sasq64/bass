#include "script.h"
#include "defines.h"

#include <coreutils/log.h>
#include <cstdio>
#include <map>
#include <sol/sol.hpp>
#include <string>

sol::state lua;

static const char* to_string = R"(
function table_print (tt, indent, done)
  print("here")
  done = done or {}
  indent = indent or 0
  if type(tt) == "table" then
    local sb = {}
    for key, value in pairs (tt) do
      table.insert(sb, string.rep (" ", indent)) -- indent it
      if type (value) == "table" and not done [value] then
        done [value] = true
        table.insert(sb, key .. " = {\n");
        table.insert(sb, table_print (value, indent + 2, done))
        table.insert(sb, string.rep (" ", indent)) -- indent it
        table.insert(sb, "}\n");
      elseif "number" == type(key) then
        table.insert(sb, string.format("\"%s\"\n", tostring(value)))
      else
        table.insert(sb, string.format(
            "%s = \"%s\"\n", tostring (key), tostring(value)))
       end
    end
    return table.concat(sb)
  else
    return tt .. "\n"
  end
end

function to_string( tbl )
    if  "nil"       == type( tbl ) then
        return tostring(nil)
    elseif  "table" == type( tbl ) then
        return table_print(tbl)
    elseif  "string" == type( tbl ) then
        return tbl
    else
        return tostring(tbl)
    end
end
)";

Scripting::Scripting()
{
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string,
                       sol::lib::math, sol::lib::table, sol::lib::debug);

    lua.script(to_string);
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

std::any to_any(sol::object const& obj)
{
    if (obj.is<Number>()) {
        return std::any(obj.as<Number>());
    }
    if (obj.is<std::string>()) {
        auto s = obj.as<std::string>();
        std::string_view sv = s;
        persist(sv);
        return std::any(sv);
    }

    if (obj.is<sol::table>()) {
        sol::table t = obj.as<sol::table>();
        Symbols syms;
        std::vector<uint8_t> vec;
        bool isVec = false;
        bool first = true;
        t.for_each([&](sol::object const& key, sol::object const& val) {
            if (first) {
                isVec = (key.is<size_t>() && val.is<Number>());
                first = false;
            }
            if (isVec) {
                auto index = key.as<size_t>() - 1;
                vec.resize(index + 1);
                vec[index] = val.as<uint8_t>();
            } else {
                auto s = key.as<std::string>();
                syms[s] = to_any(val);
            }
        });
        return isVec ? std::any(vec) : std::any(syms);
    }
    return std::any();
}

sol::object to_object(std::any const& a)
{
    if (auto* an = std::any_cast<Number>(&a)) {
        return sol::make_object(lua, *an);
    }
    if (auto* as = std::any_cast<std::string_view>(&a)) {
        return sol::make_object(lua, *as);
    }
    if (auto* av = std::any_cast<std::vector<uint8_t>>(&a)) {
        // TODO: Can we sol make this 'value' conversion?
        //return sol::make_object(lua, *av);
        sol::table t = lua.create_table();
        size_t i = 1;
        for(auto v : *av) {
            t[i++] = v;
        }
        return t;
    }
    if (auto const* at = std::any_cast<Symbols>(&a)) {
        sol::table t = lua.create_table();
        at->forAll([&](std::string const& name, std::any const& val) {
            t[name] = to_object(val);
        });
        return t;
    }
    return sol::object{};
}

std::any Scripting::call(std::string_view const& name,
                         std::vector<std::any> const& args)
{
    std::vector<sol::object> objs;
    sol::protected_function test = lua[name];

    for (auto const& a : args) {
        sol::object o = to_object(a);
        objs.push_back(o);
    }

    sol::protected_function_result fres = test.call(sol::as_args(objs));
    if (!fres.valid()) {
        sol::error err = fres;
        std::string what = err.what();
        throw script_error(what);
    }
    sol::object res = fres;
    return to_any(res);
}

