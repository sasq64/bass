#pragma once

#include "ansi_protocol.h"
#include "terminal.h"

#include <coreutils/log.h>

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
        write(protocol.set_color(cur_fg, cur_bg));
        write(protocol.goto_xy(0, 0));
        write(protocol.clear());
        write("\x1b[?25l");

        resize(terminal->width(), terminal->height());
    }
    ~Console() { write("\x1b[?25h"); }

    using ColorIndex = uint16_t;
    using Char = uint16_t;

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
    }

    void set_xy(size_t x, size_t y)
    {
        put_x = x;
        put_y = y;
    }

    void set_color(uint32_t fg, size_t bg)
    {
        put_fg = bg;
        put_bg = fg;
    }

    void put(std::string const& text)
    {
        for (auto c : text) {
            // TODO: UTF8 decode
            grid[put_x + width * put_y] = {static_cast<Char>(c), put_fg, put_bg,
                                           0};
            put_x++;
        }
    }

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
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                auto& t0 = old_grid[x + y * width];
                auto& t1 = grid[x + y * width];
                if (t0 != t1) {
                    if (cur_y != y or cur_x != x) {
                        write(protocol.goto_xy(x, y));
                        cur_x = x;
                        cur_y = y;
                    }
                    if (t1.fg != cur_fg || t1.bg != cur_bg) {
                        write(protocol.set_color(t1.fg, t1.bg));
                        cur_fg = t1.fg;
                        cur_bg = t1.bg;
                    }
                    terminal->write(std::string(1, t1.c));
                    t0 = t1;
                }
            }
        }
    }

#if 0
    enum
    {
        CODE_CRLF = 0x2028
    };

    using Char = uint16_t;

    struct Tile
    {
        explicit Tile(Char c = ' ', int fg = -1, int bg = -1)
            : fg(fg), bg(bg), c(c)
        {}
        Tile& operator=(std::initializer_list<int> il)
        {
            auto it = il.begin();
            c = *it;
            ++it;
            fg = *it;
            ++it;
            bg = *it;
            return *this;
        }
        bool operator==(const Tile& o) const
        {
            return (fg == o.fg && bg == o.bg && c == o.c);
        }
        bool operator!=(const Tile& o) const { return !(*this == o); }

        int fg;
        int bg;
        Char c;
    };

    explicit Console(Terminal& terminal)
        : terminal(terminal), fgColor(WHITE), bgColor(BLACK), width(40),
          height(25), curX(0), curY(0), raw_mode(false)
    {
        grid.resize(width * height);
        oldGrid.resize(width * height);
        clipX0 = clipY0 = 0;
        clipX1 = width;
        clipY1 = height;
        terminal.open();
    }

    virtual ~Console()
    {
        showCursor(true);
        // flush(false);
        terminal.close();
    }

    virtual int getKey(int timeout = -1);
    virtual void clear();
    virtual void put(int x, int y, const std::string& text,
                     int fg = CURRENT_COLOR, int bg = CURRENT_COLOR);
    virtual void put(int x, int y, const std::wstring& text,
                     int fg = CURRENT_COLOR, int bg = CURRENT_COLOR);
    virtual void put(int x, int y, const std::vector<uint32_t>& text,
                     int fg = CURRENT_COLOR, int bg = CURRENT_COLOR);
    virtual void put(int x, int y, Char c, int fg = CURRENT_COLOR,
                     int bg = CURRENT_COLOR);
    virtual void put(const std::string& text, int fg = CURRENT_COLOR,
                     int bg = CURRENT_COLOR);
    virtual void write(const std::string& text);
    virtual void write(const std::wstring& text);
    virtual void write(const std::vector<uint32_t>& text);

    // template <class... A> void write(const std::string &fmt, A... args) {
    //  std::string s = utils::format(fmt, args...);
    //  write(s);
    //}

    virtual void setColor(int fg, int bg = BLACK);
    virtual void resize(int w, int h);
    virtual void flush(bool restoreCursor = true);
    virtual void putChar(Char c);
    virtual void moveCursor(int x, int y);
    virtual void fill(int bg = CURRENT_COLOR, int x = 0, int y = 0,
                      int width = 0, int height = 0);

    virtual void fillLine(int y, int bg = CURRENT_COLOR)
    {
        fill(bg, 0, y, 0, 1);
    }

    virtual void refresh();

    virtual void scrollScreen(int dy);
    // virtual void scrollLine(int dx);

    virtual std::string getLine(int maxlen = 0);
    virtual std::string getLine(const std::string& prompt, int maxlen = 0)
    {
        write(prompt);
        return getLine(maxlen);
    }
    virtual std::string getPassword(int maxlen = 0);
    virtual std::string getPassword(const std::string& prompt, int maxlen = 0)
    {
        write(prompt);
        return getPassword(maxlen);
    }

    void set_raw(bool m) { raw_mode = m; }
    /*
        virtual std::future<std::string> getLineAsync() {
            getLineStarted = false;
            auto rc = std::async(std::launch::async, &Console::getLine, this);
            while(!getLineStarted) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            return rc;
        };
    */

    using vec = std::pair<int, int>;

    virtual const std::vector<Tile>& getTiles() const { return grid; }
    virtual void setTiles(const std::vector<Tile>& tiles) { grid = tiles; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    vec getSize() const { return std::make_pair(width, height); }
    int getCursorX() const { return curX; }
    int getCursorY() const { return curY; }

    std::pair<int, int> getCursor() const { return std::make_pair(curX, curY); }
    void moveCursor(const vec& pos) { moveCursor(pos.first, pos.second); }
    void crlf() { moveCursor(0, curY++); }

    void push(std::string_view const& sv);

    void showCursor(bool show);

    void clipArea(int x = 0, int y = 0, int w = -1, int h = -1)
    {
        if (w <= 0) w = width - w;
        if (h <= 0) h = height - h;
        clipX0 = x;
        clipY0 = y;
        clipX1 = x + w;
        clipY1 = y + h;
    }

    int getFg() const { return fgColor; }
    int getBg() const { return bgColor; }

    // void rawPut(Char c) {
    //  outBuffer.push_back(c & 0xff);
    //}

    virtual const std::string name() const = 0;

    static Console* createLocalConsole();

protected:
    void shiftTiles(std::vector<Tile>& tiles, int dx, int dy);
    void clearTiles(std::vector<Tile>& tiles, int x0, int y0, int w, int h);

    int get_utf8();

    // Functions that needs to be implemented by real console implementations

    /* virtual void impl_color(int fg, int bg) = 0; */
    /* virtual void impl_gotoxy(int x, int y) = 0; */
    /* virtual bool impl_scroll_screen(int dy) { return false; } */
    /* virtual bool impl_scroll_line(int dx) { return false; } */
    /* virtual int impl_handlekey() = 0; */
    /* virtual void impl_clear() = 0; */
    /* virtual void impl_showcursor(bool show) {} */

    Char translate(Char const& c) { return (c == '\t') ? ' ' : c; }

    Terminal& terminal;

    // Outgoing raw data to the terminal
    std::vector<char> outBuffer;

    // Incoming raw data from the terminal
    std::deque<char> inBuffer;

    // The contents of the screen after next flush.
    std::vector<Tile> grid;
    // The contents on the screen now. The difference is used to
    // send characters to update the console.
    std::vector<Tile> oldGrid;

    int fgColor;
    int bgColor;

    int width;
    int height;

    // The current REAL cursor position on the console
    int curX;
    int curY;

    int clipX0;
    int clipY0;
    int clipX1;
    int clipY1;

    bool raw_mode;

    // std::mutex lock;
    // std::atomic<bool> getLineStarted;
#endif
};

} // namespace bbs
