#pragma once

#include "terminal.h"

#include <coreutils/log.h>

#include <cstring>

#ifdef _WIN32

namespace bbs {

struct LocalTerminal : public Terminal
{
    size_t write(std::string_view source) override
    {
        return 0;
    }

    bool read(std::string& target) override
    {
        return false;
    }
};

}

#else

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace bbs {

struct LocalTerminal : public Terminal
{
    void open() override
    {
        struct termios new_term_attr;
        // set the terminal to raw mode
        LOGD("Setting RAW mode");
        if (tcgetattr(fileno(stdin), &orig_term_attr) < 0)
            LOGD("FAIL");
        memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
        new_term_attr.c_lflag &= ~(ECHO | ICANON);
        new_term_attr.c_cc[VTIME] = 0;
        new_term_attr.c_cc[VMIN] = 0;
        if (tcsetattr(fileno(stdin), TCSANOW, &new_term_attr) < 0)
            LOGD("FAIL");

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
            LOGD("IOCTL FAIL");

        setvbuf(stdout, NULL, _IONBF, 0);
    }

    int width() const override { return ws.ws_col; }

    int height() const override { return ws.ws_row; }

    void close() override
    {
        LOGD("Restoring terminal");
        tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);
    }

    size_t write(std::string_view source) override
    {
        return ::write(fileno(stdout), &source[0], source.length());
    }

    bool read(std::string& target) override
    {
        auto size = target.capacity();
        if(size <= 0) {
            size = 8;
            target.resize(size);
        }
        auto rc = ::read(fileno(stdin), &target[0], size-1);
        if(rc == 0) return false;
        if(rc < 0) {
            throw std::exception();
        }
        target[rc] = 0;
        return true;
    }

private:
    struct termios orig_term_attr;
    struct winsize ws;
};

} // namespace bbs

#endif