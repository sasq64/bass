#pragma once

#include "ansi_protocol.h"
#include "terminal.h"

#include <coreutils/algorithm.h>
#include <coreutils/log.h>
#include <coreutils/utf8.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bbs {

class Console
{
    using Protocol = AnsiProtocol;

public:
    enum AnsiColors
    {
        WHITE,
        RED,
        GREEN,
        BLUE,
        ORANGE,
        BLACK,
        BROWN,
        PINK,
        DARK_GREY,
        GREY,
        LIGHT_GREEN,
        LIGHT_BLUE,
        LIGHT_GREY,
        PURPLE,
        YELLOW,
        CYAN,
        CURRENT_COLOR = -2, // Use the currently set fg or bg color
        NO_COLOR = -1
    };

    AnsiProtocol protocol;

    std::unique_ptr<Terminal> terminal;

    explicit Console(std::unique_ptr<Terminal> terminal_)
    {
        terminal = std::move(terminal_);
        cur_fg = 0xc0c0c007;
        cur_bg = 0x00000000;
        put_fg = cur_fg;
        put_bg = cur_bg;
        write("\x1b[?1049h");
        write(protocol.set_color(cur_fg, cur_bg));
        write(protocol.goto_xy(0, 0));
        write(protocol.clear());
        write("\x1b[?25l");
        resize(terminal->width(), terminal->height());
    }
    ~Console()
    {
        write("\x1b[?25h");
        write("\x1b[?1049l");
    }

    using ColorIndex = uint16_t;
    using Char = char32_t;

    std::vector<uint32_t> palette;

    struct Tile
    {
        Char c = 0x20;
        uint32_t fg = 0;
        uint32_t bg = 0;
        uint16_t flags = 0;

        bool operator==(Tile const& other) const
        {
            return (other.c == c && other.fg == fg && other.bg == bg &&
                    other.flags == flags);
        }

        bool operator!=(Tile const& other) const { return !operator==(other); }
    };

    std::vector<Tile> grid;
    std::vector<Tile> old_grid;

    size_t width = 0;
    size_t height = 0;

    size_t put_x = 0;
    size_t put_y = 0;
    uint32_t put_fg = 0;
    uint32_t put_bg = 0;

    size_t cur_x = 0;
    size_t cur_y = 0;
    uint32_t cur_fg = 0;
    uint32_t cur_bg = 0;

    void resize(size_t w, size_t h)
    {
        width = w;
        height = h;
        grid.resize(w * h);
        old_grid.resize(w * h);
        utils::fill(grid, Tile{'x', 0, 0, 0});
        utils::fill(old_grid, Tile{'y', 0, 0, 0});
    }

    void set_xy(size_t x, size_t y)
    {
        put_x = x;
        put_y = y;
    }

    void set_color(uint32_t fg, uint32_t bg)
    {
        put_fg = fg;
        put_bg = bg;
    }

    void put(std::string const& text)
    {
        auto ut = utils::utf8_decode(text);
        for (auto c : ut) {
            // TODO: UTF8 decode
            grid[put_x + width * put_y] = {static_cast<Char>(c), put_fg, put_bg,
                                           0};
            put_x++;
        }
    }

    void put_char(int x, int y, Char c) { grid[x + width * y].c = c; }

    void put_char(int x, int y, Char c, uint16_t flg)
    {
        grid[x + width * y].c = c;
        grid[x + width * y].flags = flg;
    }

    void put_color(int x, int y, uint32_t fg, uint32_t bg)
    {
        grid[x + width * y].fg = fg;
        grid[x + width * y].bg = bg;
    }

    void put_color(int x, int y, uint32_t fg, uint32_t bg, uint16_t flg)
    {
        grid[x + width * y].fg = fg;
        grid[x + width * y].bg = bg;
        grid[x + width * y].flags = flg;
    }

    Tile& at(int x, int y) { return grid[x + width * y]; }

    Char get_char(int x, int y) { return grid[x + width * y].c; }

    auto get_width() const { return width; }
    auto get_height() const { return height; }

    void write(std::string_view text) const { terminal->write(text); }

    int32_t read_key() const
    {
        std::string target;
        if (terminal->read(target)) {
            return Protocol::translate_key(target);
        }
        return 0;
    }

    void flush()
    {
        int chars = 0;
        int xy = 0;
        int16_t cur_flags = 0;
        cur_x = cur_y = -1;
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                auto& t0 = old_grid[x + y * width];
                auto const& t1 = grid[x + y * width];
                if (t0 != t1) {
                    // if (cur_y != y || cur_x != x) {
                    write(protocol.goto_xy(x, y));
                    xy++;
                    cur_x = x;
                    cur_y = y;
                    //}
                    //if (t1.fg != cur_fg || t1.bg != cur_bg ||
                        //t1.flags != cur_flags) {
                        write((t1.flags & 1) != 1
                                  ? protocol.set_color(t1.fg, t1.bg)
                                  : protocol.set_color(t1.bg, t1.fg));
                        //cur_fg = t1.fg;
                        //cur_bg = t1.bg;
                        //cur_flags = t1.flags;
                    //}
                    terminal->write(utils::utf8_encode({t1.c}));
                    // cur_x++;
                    chars++;
                    t0 = t1;
                }
            }
        }
        //    LOGI("Flush %d/%d", xy, chars);
    }
};

} // namespace bbs
