#include "console.h"

//#include "editor.h"

#include <coreutils/algorithm.h>
#include <coreutils/log.h>
#include <coreutils/utf8.h>

#include <algorithm>
#include <array>

#include <fmt/format.h>

namespace bbs {

using namespace std;
using namespace utils;

struct AnsiEncoding
{

    std::string scroll_screen(int dy)
    {
        return dy > 0 ? fmt::format("\x1b[{}S", dy)
                      : fmt::format("\x1b[{}T", -dy);
    }

    std::string clear() { return "\x1b[2J"s; }

    std::string gotoxy(int x, int y)
    {
        return fmt::format("\x1b[{};{}H", y + 1, x + 1);
    }

    std::string show_cursor(bool show)
    {
        return fmt::format("\x1b[?25{}", show ? 'h' : 'l');
    }
    int handlekey(std::deque<char>) { return 0; }
};

AnsiEncoding impl;

void Console::clear()
{
    Tile t{0x20, WHITE, BLACK};
    std::fill(begin(grid), end(grid), t);
    std::fill(begin(oldGrid), end(oldGrid), t);
    push(impl.clear());
    // impl.clear();
    // curX = curY = 0;
}

void Console::push(std::string_view const& sv)
{
    utils::copy(sv, outBuffer);
}

void Console::showCursor(bool show)
{
    push(impl.show_cursor(show));
}

void Console::refresh()
{

    Tile t{0, -1, -1};
    std::fill(begin(oldGrid), end(oldGrid), t);
    push(impl.clear());
    curX = curY = 0;
    flush();
}

void Console::fill(int bg, int x, int y, int w, int h)
{

    if (x < 0) x = width + x;
    if (y < 0) y = height + y;
    if (w <= 0) w = width + w;
    if (h <= 0) h = height + h;

    if (bg == CURRENT_COLOR) bg = bgColor;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) {
            auto& t = grid[xx + width * yy];
            if (fgColor >= 0) t.fg = fgColor;
            if (bg >= 0) t.bg = bg;
            t.c = 0x20;
        }
}

void Console::put(int x, int y, Char c, int fg, int bg)
{
    if (x < 0) x = width + x;
    if (y < 0) y = height + y;

    if (y >= clipY1 || y < clipY0 || x >= clipX1 || x < clipX0) return;

    auto& t = grid[x + y * width];
    t.c = c;
    if (!raw_mode) impl.translate(t.c);

    if (fg == CURRENT_COLOR) fg = fgColor;
    if (bg == CURRENT_COLOR) bg = bgColor;

    if (fg >= 0) t.fg = fg;
    if (bg >= 0) t.bg = bg;
}

void Console::put(const std::string& text, int fg, int bg)
{
    put(curX, curY, text, fg, bg);
}

void Console::put(int x, int y, const string& text, int fg, int bg)
{
    vector<uint32_t> output(512);
    if (raw_mode) {
        output.insert(output.end(), text.begin(), text.end());
    } else {

        int l = utf8_decode(text, &output[0]);
        // int l = u8_to_ucs(text.c_str(), &output[0], 128);
        output.resize(l);
    }
    put(x, y, output, fg, bg);
}

void Console::put(int x, int y, const wstring& text, int fg, int bg)
{
    vector<uint32_t> output;
    output.insert(output.end(), text.begin(), text.end());
    put(x, y, output, fg, bg);
}

void Console::put(int x, int y, const vector<uint32_t>& text, int fg, int bg)
{

    if (x < 0) x = width + x;
    if (y < 0) y = height + y;

    if (y >= clipY1 || y < clipY0) return;

    for (int i = 0; i < (int)text.size(); i++) {

        if (x + i < clipX0) continue;
        if (x + i >= clipX1) return;

        if (text[i] > 0x100) LOGI("Ouputting %x", text[i]);

        auto& t = grid[(x + i) + y * width];
        t.c = text[i];
        if (!raw_mode) impl.translate(t.c);
        // LOGD("Putting %04x as %04x", output[i], t.c);

        if (fg == CURRENT_COLOR) fg = fgColor;
        if (bg == CURRENT_COLOR) bg = bgColor;

        if (fg >= 0) t.fg = fg;
        if (bg >= 0) t.bg = bg;
    }
}

void Console::resize(int w, int h)
{
    width = w;
    height = h;
    clipX0 = clipY0 = 0;
    clipX1 = w;
    clipY1 = h;
    LOGD("Resize");
    grid.resize(w * h);
    oldGrid.resize(w * h);
    clear();
}

void Console::flush(bool restoreCursor)
{

    auto w = terminal.getWidth();
    auto h = terminal.getHeight();
    if ((w > 0 && w != width) || (h > 0 && h != height)) {
        resize(w, h);
    }

    auto saveX = curX;
    auto saveY = curY;

    int saveFg = fgColor;
    int saveBg = bgColor;

    // auto curFg = fgColor;
    // auto curBg = bgColor;

    // TODO: Try this from clean oldGrid and clear before if more effecient

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto& t0 = oldGrid[x + y * width];
            auto& t1 = grid[x + y * width];
            if (t0 != t1) {
                if (curY != y or curX != x) {
                    impl.gotoxy(x, y);
                }
                if (t1.fg != fgColor || t1.bg != bgColor) {
                    impl.color(t1.fg, t1.bg);
                    fgColor = t1.fg;
                    bgColor = t1.bg;
                }
                putChar(t1.c);
                t0 = t1;
            }
            /*if(outBuffer.size() > 0) {
                terminal.write(outBuffer, outBuffer.size());
                outBuffer.resize(0);
                utils::sleepms(20);
            }*/
        }
    }

    if (saveFg != fgColor || saveBg != bgColor) {
        fgColor = saveFg;
        bgColor = saveBg;
        if (fgColor >= 0 && bgColor >= 0) {
            LOGD("Restoring color to %d %d", fgColor, bgColor);
            impl.color(fgColor, bgColor);
        }
    }

    // LOGD("Restorting cursor");
    if (restoreCursor) impl.gotoxy(saveX, saveY);

    if (outBuffer.size() > 0) {
        if (outBuffer.size() > 5000) {
            LOGD("WTF");
        }
        LOGV("OUTBYTES: [%02x]", outBuffer);
        terminal.write({&outBuffer[0], outBuffer.size()});
        outBuffer.resize(0);
    }
}

void Console::putChar(Char c)
{
    outBuffer.push_back(c & 0xff);
    curX++;
    if (curX >= width) {
        curX -= width;
        curY++;
    }
}

void Console::setColor(int fg, int bg)
{
    fgColor = fg;
    bgColor = bg;
    impl.color(fg, bg);
    if (outBuffer.size() > 0) {
        terminal.write({&outBuffer[0], outBuffer.size()});
        outBuffer.resize(0);
    }
}

void Console::moveCursor(int x, int y)
{

    if (x < 0) x = width + x;
    if (y < 0) y = height + y;

    if (curX == x && curY == y) return;

    impl.gotoxy(x, y);
    if (outBuffer.size() > 0) {
        terminal.write({&outBuffer[0], outBuffer.size()});
        outBuffer.resize(0);
    }
    // curX = x;
    // curY = y;
}

void Console::write(const std::wstring& text)
{
    vector<uint32_t> output;
    output.insert(output.end(), text.begin(), text.end());
    write(output);
}

void Console::write(const std::string& text)
{
    vector<uint32_t> output(128);
    int l = utf8_decode(text, &output[0]);
    //  int l = u8_to_ucs(text.c_str(), &output[0], 128);
    output.resize(l);
    write(output);
}

void Console::write(const vector<uint32_t>& text)
{

    // LOGD("Write on Y %d", curY);

    auto x = curX;
    auto y = curY;

    // LOGD("Putting %s to %d,%d", text, x, y);

    while (y >= height) {
        scrollScreen(1);
        y--;
    }
    auto spaces = 0;

    for (size_t i = 0; i < text.size(); i++) {
        // for(const auto &c : text) {
        uint32_t c;
        if (spaces) {
            spaces--;
            i--;
            c = ' ';
        } else {
            c = text[i];
            if (c == '\t') {
                spaces = 4;
                continue;
            }
        }

        if (x >= width || c == 0xa || c == 0xd) {
            x = 0;
            y++;
            if (y >= height) {
                LOGD("LF forces scroll at %d vs %d", y, curY);
                scrollScreen(1);
                y--;
            }

            if (c == 0xd) {
                if (text[i + 1] == 0xa) i++;
                c = 0xa;
            }

            if (c == 0xa) continue;
        }

        auto& t = grid[x + y * width];
        x++;
        // LOGD("put to %d %d",x+i,y);
        t.c = (Char)(c & 0xffff);
        t.c = translate(t.c);
        if (fgColor >= 0) t.fg = fgColor;
        if (bgColor >= 0) t.bg = bgColor;
        // flush();
        // this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // LOGD("%d/%d", curX, curY);
    flush();
    LOGD("Moving to %d %d", x, y);
    moveCursor(x, y);
    // LOGD("%d/%d", curX, curY);
}

int Console::getKey(int timeout)
{

    std::chrono::milliseconds ms{100};

    std::vector<uint8_t> temp;
    temp.reserve(16);

    while (true) {
        auto rc = terminal.read(16);
        for (auto const& c : rc) {
            inBuffer.push(c);
        }
        // utils::copy(rc, inBuffer);
        if (inBuffer.size() > 0) {
            // LOGD("Size %d", inBuffer.size());
            return impl.handlekey(inBuffer);
        }

        std::this_thread::sleep_for(ms);
        // utils::sleepms(100);

        if (timeout >= 0) {
            timeout -= 100;
            if (timeout < 0) return KEY_TIMEOUT;
        }
    }
}

/*
std::string Console::getLine(int maxlen) {
    auto lineEd = make_unique<LineEditor>(*this, maxlen);
    while(lineEd->update(500) != KEY_ENTER);
    if(maxlen == 0) {
        write("\n");
    }
    return lineEd->getResult();
}

std::string Console::getPassword(int maxlen) {
    auto lineEd = make_unique<LineEditor>(*this, maxlen, '*');
    while(lineEd->update(500) != KEY_ENTER);
    if(maxlen == 0) {
        write("\n");
    }
    return lineEd->getResult();
}
*/

// Shift all tiles
void Console::shiftTiles(vector<Tile>& tiles, int dx, int dy)
{
    auto tempTiles = tiles;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int tx = x - dx;
            int ty = y - dy;
            if (tx >= 0 && ty >= 0 && tx < width && ty < height)
                tiles[tx + ty * width] = tempTiles[x + y * width];
        }
    }
}

void Console::clearTiles(vector<Tile>& tiles, int x0, int y0, int w, int h)
{
    auto x1 = x0 + w;
    auto y1 = y0 + h;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            Tile& t = tiles[x + y * width];
            t.c = 0x20;
            t.fg = WHITE;
            t.bg = BLACK;
        }
    }
}

void Console::scrollScreen(int dy)
{

    shiftTiles(grid, 0, dy);
    clearTiles(grid, 0, height - dy, width, dy);
    if (impl.scroll_screen(dy)) {
        shiftTiles(oldGrid, 0, dy);
        clearTiles(oldGrid, 0, height - dy, width, dy);
    }
    flush();
}

} // namespace bbs

#ifdef UNIT_TEST

#    include "catch.hpp"
#    include <sys/time.h>

using namespace bbs;

class TestTerminal : public Terminal
{
public:
    virtual int write(const std::vector<Char>& source, int len)
    {
        for (int i = 0; i < len; i++)
            outBuffer.push_back(source[i]);
        return len;
    }
    virtual int read(std::vector<Char>& target, int len)
    {
        int rc = -1; // outBuffer.size();
        // target.insert(target.back, outBuffer.begin(), outBuffer.end());
        // outBuffer.resize(0);
        return rc;
    }
    std::vector<Char> outBuffer;
};
/*
TEST_CASE("console::basic", "Console") {

    TestTerminal terminal;
    PetsciiConsole console { terminal };

    //1b 5b 32 4a 1b 5b 32 3b 32 48  74 65 73 74 69 6e 67
    console.setFg(Console::WHITE);
    console.setBg(Console::BLACK);
    console.put(37,1, "abcdefghijk");
    console.put(0, 3, "ABCDEFGH");
    console.flush();
    string s = utils::format("[%02x]\n", terminal.outBuffer);
    printf(s.c_str());


    REQUIRE(true);

}
*/
#endif
