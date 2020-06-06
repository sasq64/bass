
#include <coreutils/path.h>

#include <any>
#include <string>
#include <vector>

class script_error : public std::exception
{
public:
    explicit script_error(std::string m = "Script error") : msg(std::move(m))
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};


class Scripting
{
public:
    Scripting();
    void load(utils::path const& p);
    void add(std::string_view const& code);
    bool hasFunction(std::string_view const& name);
    std::any call(std::string_view const& name, std::vector<std::any> const& args);
};
