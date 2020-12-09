#pragma once

#include "terminal.h"
#include <coreutils/log.h>

#include <io.h>
#include <cstring>


namespace bbs {

struct LocalTerminal : public Terminal
{
    size_t write(std::string_view source) override
    {
        return _write(_fileno(stdout), &source[0], source.length());
    }

    bool read(std::string& target) override
    {
        auto size = target.capacity();
        if(size <= 0) {
            size = 8;
            target.resize(size);
        }
        auto rc = _read(_fileno(stdin), &target[0], size-1);
        if(rc == 0) return false;
        if(rc < 0) {
            throw std::exception();
        }
        target[rc] = 0;
        return true;
    }
};

} // namespace bbs
