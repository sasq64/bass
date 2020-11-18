#pragma once

#include <defines.h>
#include <coreutils/file.h>

static fs::path findProjectDir()
{
    auto current = fs::absolute(".");

    while(!current.empty()) {
        if (fs::exists(current / ".git")) {
            return current;
        }
        current = current.parent_path();
    }
    return {};
}

inline fs::path projDir()
{
   static fs::path projectDir = findProjectDir();
   return projectDir;
}

