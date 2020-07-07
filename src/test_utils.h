#pragma once

#include <coreutils/file.h>
#include <coreutils/path.h>

static utils::path findProjectDir()
{
    auto current = utils::absolute(".");

    while(!current.empty()) {
        if (utils::exists(current / ".git")) {
            return current;
        }
        current = current.parent_path();
    }
    return {};
}

inline utils::path projDir()
{
   static utils::path projectDir = findProjectDir();
   return projectDir;
}

inline utils::path testFilePath(std::string const& name)
{
   return projDir() / "data" / "test" / name;

}

inline std::string readTestFile(std::string const& name)
{
   return utils::File{projDir() / "data" / "test" / name}.readAllString();

}
