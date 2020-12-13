#include "pet100.h"

#include <ansi/console.h>
#include <ansi/terminal.h>
#include <coreutils/algorithm.h>
#include <coreutils/utf8.h>
#include <thread>

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

void Pet100::set_color(uint8_t col)
{
    int b = (col & 0xf) * 3;
    int f = ((col >> 4) + 16) * 3;
    uint32_t fg =
        (palette[f] << 24) | (palette[f + 1] << 16) | (palette[f + 2] << 8);
    uint32_t bg =
        (palette[b] << 24) | (palette[b + 1] << 16) | (palette[b + 2] << 8);
    console->set_color(fg, bg);
}

void Pet100::writeChar(uint16_t adr, uint8_t t)
{
    if (regs[RealW] == 0 || regs[RealH] == 0) {
        return;
    }
    auto offset = adr - (regs[TextPtr] * 256);
    //textRam[offset] = t;

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

    int b = (c & 0xf) * 3;
    int f = ((c >> 4) + 16) * 3;
    uint32_t fg =
        (palette[f] << 24) | (palette[f + 1] << 16) | (palette[f + 2] << 8);
    uint32_t bg =
        (palette[b] << 24) | (palette[b + 1] << 16) | (palette[b + 2] << 8);
    console->put_color(x, y, fg, bg);
}

void Pet100::updateRegs()
{

    auto sz = (regs[WinW] * regs[WinH] + 255) & 0xffff00;
    auto banks = sz / 256;

    emu->map_write_callback(regs[TextPtr], banks, this,
                            [](uint16_t adr, uint8_t v, void* data) {
                                auto* thiz = static_cast<Pet100*>(data);
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
    auto cw = console->get_width();
    auto ch = console->get_height();
    set_color(col);
    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            if (x < regs[WinX] || x >= (regs[WinX] + regs[WinW]) ||
                y < regs[WinY] || y >= (regs[WinY] + regs[WinH])) {
                console->set_xy(x, y);
                console->put(" ");
            }
        }
    }
}

Pet100::Pet100()
{
    auto terminal = bbs::create_local_terminal();
    terminal->open();
    console = std::make_unique<bbs::Console>(std::move(terminal));
    logging::setAltMode(true);

    auto cw = console->get_width();
    auto ch = console->get_height();

    textRam.resize(cw*ch);
    colorRam.resize(cw*ch);

    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            //colorRam[x + cw * y] = 0x01;
            //textRam[x + cw * y] = ' ';
            console->at(x,y) = { ' ', 0x000000, 0xffffffff, 0 };
        }
    }

    emu = std::make_unique<sixfive::Machine<>>();

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
    regs[Freq] = 20;
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
        if(update())
            return;
    }
}

uint16_t Pet100::get_ticks() const
{
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(clk::now() -
                                                                    start_t);
    return (ms.count() / 100) & 0xffff;
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
    case TimerLo:
        return get_ticks() & 0xff;
    case TimerHi:
        return (get_ticks() >> 8) & 0xff;
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
    case CFillOut:
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

void Pet100::doUpdate()
{
    console->flush();

    auto oldR = regs[IrqR];
    regs[IrqR] |= 1;

    if ((~oldR & regs[IrqR]) != 0) {
        if ((regs[IrqR] & regs[IrqE]) != 0) {
            emu->irq(emu->read_mem(0xfffc) | (emu->read_mem(0xfffd) << 8));
        }
    }
}

bool Pet100::update()
{
    try {
        auto cycles = emu->run(10000);
        if (clk::now() >= nextUpdate) {
            nextUpdate += 20ms;
            doUpdate();
        } else {
            std::this_thread::sleep_for(1ms);
        }
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
    nextUpdate = start_t + 10ms;
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
