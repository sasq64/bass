#pragma once

#include <coreutils/path.h>

#include <any>
#include <functional>
#include <memory>
#include <sol/forward.hpp>
#include <string>
#include <vector>

class script_error : public std::exception
{
public:
    explicit script_error(std::string m = "Script error") : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

class Scripting
{
public:
    Scripting();
    ~Scripting();
    void load(utils::path const& p);
    void add(std::string_view const& code);
    bool hasFunction(std::string_view const& name);
    std::any call(std::string_view const& name,
                  std::vector<std::any> const& args);

    sol::state& getState() { return *luap; }

    sol::object to_object(std::any const& a);
    std::any to_any(sol::object const& obj);

    static constexpr int StartIndex = 1;
    std::function<void()> make_function(std::string_view const& code);

private:
    std::unique_ptr<sol::state> luap;
    sol::state& lua;

};
