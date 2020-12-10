#pragma once

#include "terminal.h"
#include <coreutils/log.h>

#include <conio.h>
#include <io.h>
#include <windows.h>

#include <cstring>

namespace bbs {

struct LocalTerminal : public Terminal
{
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    HANDLE hOut;
    DWORD savedMode = 0;

    int width() const override { return csbiInfo.dwSize.X; }

    int height() const override { return csbiInfo.dwSize.Y; }

    bool init()
    {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        if (hOut == INVALID_HANDLE_VALUE) {
            return false;
        }

        if (!GetConsoleMode(hOut, &savedMode)) {
            return false;
        }

        if (!SetConsoleMode(hOut, savedMode |
                                      ENABLE_VIRTUAL_TERMINAL_PROCESSING
                                      )) {
            return false;
        }

        GetConsoleScreenBufferInfo(hOut, &csbiInfo);
        return true;
    }

    void open() override { init(); }

    void close() override
    {
        if (!SetConsoleMode(hOut, savedMode)) {
            return;
        }
    }

    size_t write(std::string_view source) override
    {
        return _write(_fileno(stdout), &source[0], source.length());
    }

    bool read(std::string& target) override
    {
        auto size = target.capacity();
        if (size <= 0) {
            size = 8;
            target.resize(size);
        }
        auto rc = _read(_fileno(stdin), &target[0], size - 1);
        if (rc == 0) return false;
        if (rc < 0) {
            throw std::exception();
        }
        target[rc] = 0;
        return true;
    }
};

} // namespace bbs
