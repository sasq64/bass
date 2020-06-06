
#include <coreutils/path.h>

#include <any>
#include <string>
#include <vector>

class Scripting
{
public:
    Scripting();
    void load(utils::path const& p);
    void add(std::string_view const& code);
    bool hasFunction(std::string_view const& name);
    std::any call(std::string_view const& name, std::vector<std::any> const& args);
};
