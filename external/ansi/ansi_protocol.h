#include <fmt/format.h>
#include <cstdint>
#include <string>

struct AnsiProtocol
{
    std::string goto_xy(size_t x, size_t y)
    {
        return fmt::format("\x1b[{};{}H", y + 1, x + 1);
    }

    // 0x000000xx -> 0xffffffxx
    // top 24 bits = true color
    // low 8 bits is color index.
    // If terminal can use RGB it should
    // If RGB != 0 AND index == 0, assume RGB must be used
    std::string set_color(uint32_t fg, uint32_t bg)
    {
        unsigned r0 = fg >> 24;
        unsigned g0 = (fg >> 16) & 0xff;
        unsigned b0 = (fg >> 8) & 0xff;
        unsigned r1 = bg >> 24;
        unsigned g1 = (bg >> 16) & 0xff;
        unsigned b1 = (bg >> 8) & 0xff;
        return fmt::format("\x1b[38;2;{};{};{}m\x1b[48;2;{};{};{}m", r0, g0, b0, r1, g1, b1);
    }

    std::string clear()
    {
        return "\x1b[2J";
    }

};