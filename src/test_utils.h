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

