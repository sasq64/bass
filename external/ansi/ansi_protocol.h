#include "keycodes.h"

#include <coreutils/log.h>
#include <cstdint>
#include <fmt/format.h>
#include <string>

struct AnsiProtocol
{
    static std::string goto_xy(size_t x, size_t y)
    {
        return fmt::format("\x1b[{};{}H", y + 1, x + 1);
    }

    // 0x000000xx -> 0xffffffxx
    // top 24 bits = true color
    // low 8 bits is color index.
    // If terminal can use RGB it should
    // If RGB != 0 AND index == 0, assume RGB must be used
    static std::string set_color(uint32_t fg, uint32_t bg)
    {
        unsigned r0 = fg >> 24;
        unsigned g0 = (fg >> 16) & 0xff;
        unsigned b0 = (fg >> 8) & 0xff;
        unsigned r1 = bg >> 24;
        unsigned g1 = (bg >> 16) & 0xff;
        unsigned b1 = (bg >> 8) & 0xff;
        return fmt::format("\x1b[38;2;{};{};{}m\x1b[48;2;{};{};{}m", r0, g0, b0,
                           r1, g1, b1);
    }

    static std::string clear() { return "\x1b[2J"; }

    static uint32_t translate_key(std::string_view seq)
    {
        auto pop = [&] {
            auto c = seq.front();
            seq.remove_prefix(1);
            return static_cast<uint8_t>(c);
        };

        auto c = pop();
        if (c >= 0x80) {
            return KEY_UNKNOWN;
        }

        if (c != 0x1b) {
            if (c == 13 || c == 10) {
                if (c == 13) {
                    auto c2 = pop();
                    if (c2 == 10) {
                        pop();
                    }
                }
                return KEY_ENTER;
            }
            if (c == 0x7f) return KEY_BACKSPACE;
            if (c == 0x7e) return KEY_DELETE;
            return c;
        }

        if (!seq.empty()) {
            auto c2 = pop();
            auto c3 = pop();

            //LOGI("ESCAPE key %02x %02x %02x", c, c2, c3);

            if (c2 == 0x5b || c2 == 0x4f) {
                switch (c3) {
                case 0x50:
                    return KEY_F1;
                case 0x51:
                    return KEY_F2;
                case 0x52:
                    return KEY_F3;
                case 0x53:
                    return KEY_F4;
                case 0x31:
                    if (seq.size() >= 2) {
                        auto c4 = pop();
                        auto c5 = pop();
                        if (c5 == 126) {
                            switch (c4) {
                            case '5':
                                return KEY_F5;
                            case '7':
                                return KEY_F6;
                            case '8':
                                return KEY_F7;
                            case '9':
                                return KEY_F8;
                            default:
                                return KEY_UNKNOWN;
                            }
                        }
                    }
                    break;
                case 0x33:
                    if (!seq.empty() && seq.front() == 126) pop();
                    return KEY_DELETE;
                case 0x48:
                    return KEY_HOME;
                case 0x46:
                    return KEY_END;
                case 0x44:
                    return KEY_LEFT;
                case 0x43:
                    return KEY_RIGHT;
                case 0x41:
                    return KEY_UP;
                case 0x42:
                    return KEY_DOWN;
                case 0x35:
                    pop();
                    return KEY_PAGEUP;
                case 0x36:
                    pop();
                    return KEY_PAGEDOWN;
                default:
                    return KEY_UNKNOWN;
                }
            }
        }
        return c;
    }
};