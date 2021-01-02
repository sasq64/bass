#include "pet100.h"

#include <ansi/console.h>
#include <ansi/terminal.h>
#include <cctype>
#include <coreutils/algorithm.h>
#include <coreutils/file.h>
#include <coreutils/utf8.h>
#include <thread>

#include "emu_policy.h"
#include "emulator.h"
#include "petscii.h"

#include <array>
#include <chrono>

using namespace std::chrono_literals;
static constexpr std::array<uint8_t, 16 * 3> c64pal = {
    0x00, 0x00, 0x00, // BLACK
    0xFF, 0xFF, 0xFF, // WHITE
    0x68, 0x37, 0x2B, // RED
    0x70, 0xA4, 0xB2, // CYAN
    0x6F, 0x3D, 0x86, // PURPLE
    0x58, 0x8D, 0x43, // GREEN
    0x35, 0x28, 0x79, // BLUE
    0xB8, 0xC7, 0x6F, // YELLOW
    0x6F, 0x4F, 0x25, // ORANGE
    0x43, 0x39, 0x00, // BROWN
    0x9A, 0x67, 0x59, // LIGHT_READ
    0x44, 0x44, 0x44, // DARK_GREY
    0x6C, 0x6C, 0x6C, // GREY
    0x9A, 0xD2, 0x84, // LIGHT_GREEN
    0x6C, 0x5E, 0xB5, // LIGHT_BLUE
    0x95, 0x95, 0x95, // LIGHT_GREY
};

std::string translate(uint8_t c)
{
    char32_t u = sc2uni_up(c);
    std::u32string s = {u};
    return utils::utf8_encode(s);
}

char32_t trans_char(uint8_t c)
{
    return sc2uni_up(c);
}

void Pet100::writeChar(uint16_t adr, uint8_t t)
{
    if (regs[RealW] == 0 || regs[RealH] == 0) {
        return;
    }
    auto offset = adr - (regs[TextPtr] * 256);
    // textRam[offset] = t;

    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    textRam[x + regs[RealW] * y] = t;

    uint16_t flags = (t & 0x80) != 0 ? 1 : 0;
    console->put_char(x, y, trans_char(t), flags);
}

uint8_t Pet100::readChar(uint16_t adr)
{
    auto offset = adr - (regs[TextPtr] * 256);

    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    return textRam[x + regs[RealW] * y];
}

uint8_t Pet100::readColor(uint16_t adr)
{
    auto offset = adr - (regs[ColorPtr] * 256);

    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    return colorRam[x + regs[RealW] * y];
}

void Pet100::writeColor(uint16_t adr, uint8_t c)
{
    if (regs[RealW] == 0 || regs[RealH] == 0) {
        return;
    }
    auto offset = adr - (regs[ColorPtr] * 256);
    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    colorRam[x + regs[RealW] * y] = c;

    int f = (c & 0xf) * 3;
    int b = ((c >> 4) + 16) * 3;
    uint32_t fg =
        (palette[f] << 24) | (palette[f + 1] << 16) | (palette[f + 2] << 8);
    uint32_t bg =
        (palette[b] << 24) | (palette[b + 1] << 16) | (palette[b + 2] << 8);
    console->put_color(x, y, fg, 0);
}

void Pet100::updateRegs()
{

    auto sz = (regs[WinW] * regs[WinH] + 255) & 0xffff00;
    auto banks = sz / 256;

    emu->map_write_callback(regs[TextPtr], banks, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
                                // LOGE("%02x -> %04x", v, adr);
                                thiz->writeChar(adr, v);
                            });
    emu->map_read_callback(regs[TextPtr], banks, this,
                           [](uint16_t adr, void* data) {
                               auto* thiz = static_cast<Pet100*>(data);
                               return thiz->readChar(adr);
                           });
    emu->map_write_callback(regs[ColorPtr], banks, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
                                // LOGE("%02x -> %04x", v, adr);
                                thiz->writeColor(adr, v);
                            });
    emu->map_read_callback(regs[ColorPtr], banks, this,
                           [](uint16_t adr, void* data) {
                               auto* thiz = static_cast<Pet100*>(data);
                               return thiz->readColor(adr);
                           });
}

void Pet100::fillOutside(uint8_t col)
{
    return;
    auto cw = console->get_width();
    auto ch = console->get_height();
    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            if (x < regs[WinX] || x >= (regs[WinX] + regs[WinW]) ||
                y < regs[WinY] || y >= (regs[WinY] + regs[WinH])) {
                console->put_char(x, y, ' ');
                console->put_color(x, y, col, col);
            }
        }
    }
}

struct NullTerminal : public bbs::Terminal
{
    size_t write(std::string_view source) override { return source.size(); }

    bool read(std::string& target) override { return false; }
    int width() const override { return 40; }
    int height() const override { return 25; }
};

Pet100::Pet100()
{
    auto terminal = bbs::create_local_terminal();
    // auto terminal = std::make_unique<NullTerminal>();
    terminal->open();
    console = std::make_unique<bbs::Console>(std::move(terminal));
    logging::setAltMode(true);
    logging::setLevel(logging::Level::Error);

    auto cw = console->get_width();
    auto ch = console->get_height();

    textRam.resize(cw * ch);
    colorRam.resize(cw * ch);

    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            // colorRam[x + cw * y] = 0x01;
            // textRam[x + cw * y] = ' ';
            console->at(x, y) = {' ', 0x000000, 0xffffffff, 0};
        }
    }

    emu = std::make_unique<sixfive::Machine<EmuPolicy>>();

    static Intercept i;
    i.fn = [&](uint16_t a) {
        LOGE("%04x: A=%x X=%x Y=%x", emu->regPC(), emu->get<sixfive::Reg::A>(),
             emu->get<sixfive::Reg::X>(), emu->get<sixfive::Reg::Y>());
        return false;
    };

    /* emu->policy().intercepts[0xa31] = &i; */
    /* emu->policy().intercepts[0xa87] = &i; */
    /* emu->policy().intercepts[0xa96] = &i; */
    /* emu->policy().intercepts[0xab3] = &i; */
    /* emu->policy().intercepts[0xab9] = &i; */

    utils::File bf{"basic"};
    basic = bf.readAll();
    utils::File kf{"kernal"};
    kernal = kf.readAll();

    // kernal[0x1ff2] = 0x00;

    emu->map_rom(0xa0, basic.data(), 0x2000);
    emu->map_rom(0xe0, kernal.data(), 0x2000);
    //static auto cia_start = clk::now();
    static int counter = 0;

    emu->map_read_callback(0xd0, 0xc, this,
                           [](uint16_t adr, void* data) -> uint8_t {
                               auto* thiz = static_cast<Pet100*>(data);
                               LOGI("Read %x", adr);
                               if (adr == 0xd012) {
                                   return counter;
                               }
                               return 0;
                           });
    emu->map_write_callback(0xd0, 0xc, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
                                if (adr == 0xd020) {
                                    thiz->writeReg(8, v << 4);
                                }
                                LOGI("Write %x -> %x", v, adr);
                            });

    emu->map_read_callback(0xdc, 1, this,
                           [](uint16_t adr, void* data) -> uint8_t {
                               auto* thiz = static_cast<Pet100*>(data);
                               LOGI("CIA A Read %x", adr);
                               if (adr == 0xdc01) {
                                   // Return the column
                                   uint8_t res = 0;
                                   // LOGE("Mask %x", thiz->ciaa[0]);
                                   for (int i = 0; i < 8; i++) {
                                       if ((1 << i) & ~(thiz->ciaa[0x00])) {
                                           // We want to return column i
                                           // LOGE("%d -> %x", i,
                                           // thiz->pressed[i]);
                                           res |= thiz->pressed[i];
                                       }
                                   }
                                   return ~res;
                               }
                               return 0;
                           });
    emu->map_write_callback(0xdc, 1, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
                                LOGI("CIA A Write %x -> %x", v, adr);
                                thiz->ciaa[adr - 0xdc00] = v;
                            });

    emu->map_read_callback(0xdd, 2, this,
                           [](uint16_t adr, void* data) -> uint8_t {
                               auto* thiz = static_cast<Pet100*>(data);
                               if (adr == 0xdd00 || adr == 0xdd01) {
                                   return 0xff;
                               }
                               LOGI("CIA B Read %x", adr);
                               return 0;
                           });
    emu->map_write_callback(0xdd, 2, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
                                LOGI("CIA B Write %x -> %x", v, adr);
                            });
    // Map IO area
    emu->map_read_callback(0xd7, 1, this,
                           [](uint16_t adr, void* data) -> uint8_t {
                               auto* thiz = static_cast<Pet100*>(data);
                               if (adr >= 0xd780) {
                                   return thiz->palette[adr - 0xd780];
                               }
                               return thiz->readReg(adr & 0xff);
                           });

    emu->map_write_callback(0xd7, 1, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
                                if (adr < 0xd780) {
                                    thiz->writeReg(adr & 0xff, v);
                                } else {
                                    thiz->palette[adr - 0xd780] = v;
                                }
                            });
    regs[WinH] = 25;
    regs[WinW] = 40;
    regs[WinX] = (cw - regs[WinW]) / 2;
    regs[WinY] = (ch - regs[WinH]) / 2;
    regs[RealW] = cw;
    regs[RealH] = ch;

    regs[TextPtr] = 0x04;
    regs[ColorPtr] = 0xd8;
    regs[TimerDiv] = 10;
    regs[IrqE] = 0;
    regs[IrqR] = 0;

    updateRegs();
    utils::fill(textRam, 0x20);
    utils::fill(colorRam, 0x01);
    utils::copy(c64pal, palette.data());
    utils::copy(c64pal, palette.data() + 16 * 3);
}

void Pet100::run(uint16_t pc)
{
    start(pc);
    while (true) {
        if (update()) return;
    }
}

uint32_t Pet100::get_ticks() const
{
    auto us = frozenTimer
                  ? frozenTime
                  : std::chrono::duration_cast<std::chrono::microseconds>(
                        clk::now() - start_t);
    return (us.count() / regs[TimerDiv]) & 0xffff;
}

void Pet100::freeze_timer(bool freeze)
{
    if (freeze && !frozenTimer) {
        frozenTime = std::chrono::duration_cast<std::chrono::microseconds>(
            clk::now() - start_t);
    }
    frozenTimer = freeze;
}

uint8_t Pet100::readReg(int reg)
{
    switch (reg) {
    case WinX:
    case WinY:
    case WinW:
    case WinH:
    case TextPtr:
    case ColorPtr:
    case Charset:
    case IrqR:
    case IrqE:
        return regs[reg];
    case Keys:
        if (auto key = console->read_key()) {
            return key;
        }
        return 0;
    case TimerLo: {
        uint8_t r = get_ticks() & 0xff;
        freeze_timer(false);
        return r;
    }
    case TimerMid:
        freeze_timer(true);
        return (get_ticks() >> 8) & 0xff;
    case TimerHi:
        freeze_timer(true);
        return (get_ticks() >> 16) & 0xff;
    case RealW:
        return console->get_width();
    case RealH:
        return console->get_height();
    default:
        return 0;
    }
}

void Pet100::writeReg(int reg, uint8_t val)
{
    switch (reg) {
    case WinX:
    case WinY:
    case WinW:
    case WinH:
    case TextPtr:
    case ColorPtr:
        regs[reg] = val;
        updateRegs();
        break;
    case Charset:
        regs[reg] = val;
        break;
    case Border:
        fillOutside(val);
        break;
    case Control:
        if ((val & 1) != 0) {
            throw exit_exception();
        } else if ((val & 2) != 0) {
            std::this_thread::sleep_until(nextUpdate);
            doUpdate();
        }
        break;
    case IrqR:
        regs[IrqR] ^= val;
        break;
    default:
        break;
    }
}

// clang-format off
enum KeysC64 {
    DOWN, F5, F3, F1, F7, RIGHT, RETURN, DELETE, // COL0
    LSHIFT, E, S, Z, FOUR, A, W, THREE, // COL1
    X, T, F, C, SIX, D, R, FIVE,
    V, U, H, B, EIGHT, G, Y, SEVEN,
    N, O, K, M, ZERO, J, I, NINE,
    COMMA, AT, COLON, DOT, DASH, L, P, PLUS,
    SLASH, UARROW, EQ, RSHIFT, HOME, SEMI, STAR, POUND,
    STOP, Q, COM, SPACE, TWO, CTRL, LARROW, ONE,
//  ROW7 .................................. ROW0
};
// clang-format on

static std::unordered_map<int32_t, int32_t> setup()
{
    std::unordered_map<int32_t, int32_t> m;
    std::vector<int> letters = {A, B, C, D, E, F, G, H, I, J, K, L, M,
                                N, O, P, Q, R, S, T, U, V, W, X, Y, Z};
    std::vector<int> numbers = {ZERO, ONE, TWO,   THREE, FOUR,
                                FIVE, SIX, SEVEN, EIGHT, NINE};

    char c = 'a';
    for (auto const& l : letters) {
        m.emplace(c, l);
        m.emplace(toupper(c), l | LSHIFT << 8);
        c++;
    }
    c = '0';
    for (auto const& l : numbers) {
        m.emplace(c, l);
        c++;
    }
    const char* x = "0!\"#$%&'()";
    int i = 0;
    for (auto const& l : numbers) {
        m.emplace(x[i], l | (LSHIFT << 8));
        i++;
    }

    m.emplace(KEY_LEFT, (RSHIFT << 8) | RIGHT);
    m.emplace(KEY_RIGHT, RIGHT);

    m.emplace(KEY_UP, (RSHIFT << 8) | DOWN);
    m.emplace(KEY_DOWN, DOWN);

    // m.emplace(':', (LSHIFT<<8) | COLON);
    m.emplace(':', COLON);

    m.emplace(']', (LSHIFT << 8) | SEMI);
    m.emplace('[', (LSHIFT << 8) | COLON);
    m.emplace(';', SEMI);
    m.emplace('=', EQ);
    m.emplace('+', PLUS);
    m.emplace('-', DASH);
    m.emplace('*', STAR);
    m.emplace('@', AT);
    m.emplace('/', SLASH);
    m.emplace('?', (LSHIFT << 8) | SLASH);
    // m.emplace(']', SEMI);

    m.emplace(KEY_ENTER, RETURN);
    m.emplace(KEY_BACKSPACE, DELETE);
    m.emplace(' ', SPACE);
    m.emplace(KEY_ESCAPE, STOP);
    return m;
}

static auto conv = setup();

// Column selected via write,
// Return column

std::array<uint8_t, 64> pressed{};

void translateKey(int32_t key) {}

void Pet100::doUpdate()
{
    //    console->flush();
    //
    pressed = {0, 0, 0, 0, 0, 0, 0, 0};
    auto key = console->read_key();
    if (key != 0) {
        auto c64 = conv[key];
        do {
            pressed[(c64 & 0xff) / 8] |= (0x80 >> ((c64 & 0xff) % 8));
            c64 >>= 8;
        } while (c64 != 0);
        // currentKey = key;
    }

    // auto oldR = regs[IrqR];
    // regs[IrqR] |= 1;

    // if ((~oldR & regs[IrqR]) != 0) {
    // if ((regs[IrqR] & regs[IrqE]) != 0) {
    if ((emu->get<sixfive::Reg::SR>() & 0x4) == 0) {
        // logging::setLevel(logging::Level::Warning);
        emu->irq(emu->read_mem(0xfffe) | (emu->read_mem(0xffff) << 8));
    }
    // }
    //}
}

bool Pet100::update()
{
    try {
        auto cycles = emu->run(10000);
        if (clk::now() >= nextUpdate) {
            console->flush();
            nextUpdate += 20ms;
            doUpdate();
        } else {
            std::this_thread::sleep_for(1ms);
        }
        return false;
        // if(cycles != 0) {
        //    LOGE("Cycles %d", cycles);
        //}
        return cycles != 0;
    } catch (exit_exception&) {
        return true;
    }
}

void Pet100::load(uint16_t start, uint8_t const* ptr, size_t size) const
{
    constexpr uint8_t Sys_Token = 0x9e;
    constexpr uint8_t Space = 0x20;

    if (start == 0x0801) {
        const auto* p = ptr;
        size_t sz = size > 12 ? 12 : size;
        const auto* endp = ptr + sz;

        while (*p != Sys_Token && p < endp)
            p++;
        p++;
        while (*p == Space && p < endp)
            p++;
        if (p < endp) {
            basicStart = std::atoi(reinterpret_cast<char const*>(p));
            fmt::print("Detected basic start at {:04x}", basicStart);
        }
    }
    emu->write_memory(start, ptr, size);
}

void Pet100::start(uint16_t pc)
{
    if (pc == 0x0801 && basicStart >= 0) {
        pc = basicStart;
    }

    fillOutside(0x01);

    emu->setPC(pc);
    start_t = clk::now();
    nextUpdate = start_t + 1ms;
}

int Pet100::get_width() const
{
    return console->get_width();
}
int Pet100::get_height() const
{
    return console->get_height();
}

Pet100::~Pet100() = default;
