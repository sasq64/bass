#include "pet100.h"

#include "emulator.h"
#include "machine.h"
#include "petscii.h"

#include <ansi/console.h>
#include <ansi/terminal.h>
#include <coreutils/algorithm.h>
#include <coreutils/log.h>
#include <coreutils/utf8.h>


#include <array>
#include <chrono>
#include <thread>

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
    char32_t const u = sc2uni_up(c);
    std::u32string const s = {u};
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
    unsigned offset = adr - (regs[TextPtr] * 256);
    // textRam[offset] = t;

    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    if (offset >= textRam.size()) { return; }
    textRam[offset] = t;

    uint16_t flags = (t & 0x80) != 0 ? 1 : 0;
    console->put_char(x, y, trans_char(t), flags);
}

uint8_t Pet100::readChar(uint16_t adr)
{
    unsigned offset = adr - (regs[TextPtr] * 256);

    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];
    offset = x + regs[RealW] * y;

    if (offset >= textRam.size()) { return 0; }
    return textRam[x + regs[RealW] * y];
}

uint8_t Pet100::readColor(uint16_t adr)
{
    unsigned  offset = adr - (regs[ColorPtr] * 256);

    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    offset = x + regs[RealW] * y;
    if (offset >= colorRam.size()) { return 0; }
    return colorRam[offset];
}

void Pet100::writeColor(uint16_t adr, uint8_t c)
{
    if (regs[RealW] == 0 || regs[RealH] == 0) {
        return;
    }
    unsigned offset = adr - (regs[ColorPtr] * 256);
    auto x = (offset % regs[WinW] + regs[WinX]) % regs[RealW];
    auto y = (offset / regs[WinW] + regs[WinY]) % regs[RealH];

    offset = x + regs[RealW] * y;
    if (offset >= colorRam.size()) { return; }

    colorRam[offset] = c;

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

    mach.setBankWrite(regs[TextPtr], banks,
                      [this](uint16_t adr, uint8_t v) { writeChar(adr, v); });
    mach.setBankRead(regs[TextPtr], banks,
                     [this](uint16_t adr) { return readChar(adr); });

    mach.setBankWrite(regs[ColorPtr], banks,
                      [this](uint16_t adr, uint8_t v) { writeColor(adr, v); });
    mach.setBankRead(regs[ColorPtr], banks,
                     [this](uint16_t adr) { return readColor(adr); });
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

class DummyTerminal : public bbs::Terminal
{
    size_t write(std::string_view source) override { return source.length(); }

    bool read(std::string& target) override { return false; }

    int width() const override { return 40; }
    int height() const override { return 25; }
};

Pet100::Pet100(Machine& m, bool noScreen) : mach(m)
{
    using sixfive::Reg;
    int cw = 40;
    int ch = 25;
    if (noScreen) {
        std::unique_ptr<bbs::Terminal> terminal = std::make_unique<DummyTerminal>();
        console = std::make_unique<bbs::Console>(std::move(terminal));
    } else {
        auto terminal = bbs::create_local_terminal();
        terminal->open();
        console = std::make_unique<bbs::Console>(std::move(terminal));
        logging::setAltMode(true);
        cw = static_cast<int>(console->get_width());
        ch = static_cast<int>(console->get_height());
        if (cw <= 0 || ch <= 0) {
            fmt::print("Illegal console size\n");
            exit(1);
        }
    }

    textRam.resize(cw * ch);
    colorRam.resize(cw * ch);

    for (int y = 0; y < ch; y++) {
        for (int x = 0; x < cw; x++) {
            // colorRam[x + cw * y] = 0x01;
            // textRam[x + cw * y] = ' ';
            console->at(x, y) = {' ', 0x000000, 0xffffffff, 0};
        }
    }

    mach.setBankRead(0xd7, 1,
                     // Map IO area
                     [this](uint16_t adr) -> uint8_t {
                         // auto* thiz = static_cast<Pet100*>(data);

                         if (adr >= 0xd780) {
                             return palette[adr - 0xd780];
                         }
                         return readReg(adr & 0xff);
                     });

    mach.setBankWrite(0xd7, 1, [this](uint16_t adr, uint8_t v) {
        // auto* thiz = static_cast<Pet100*>(data);
        if (adr < 0xd780) {
            writeReg(adr & 0xff, v);
        } else {
            palette[adr - 0xd780] = v;
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

void Pet100::doUpdate()
{
    console->flush();

    auto oldR = regs[IrqR];
    regs[IrqR] |= 1;

    auto sr = mach.getReg(sixfive::Reg::SR);

    if ((sr & 4) == 0) {
        //if ((~oldR & regs[IrqR]) != 0) {
        //    if ((regs[IrqR] & regs[IrqE]) != 0) {
                auto adr = mach.readRam(0xfffc) | (mach.readRam(0xfffd)<<8);
                //logging::log("IRQ at %04x", adr);
                mach.doIrq(adr);
        //    }
       // }
    }
}

bool Pet100::update()
{
    try {
        auto done = mach.runCycles(10000);
        if (clk::now() >= nextUpdate) {
            nextUpdate += 20ms;
            doUpdate();
        } else {
            std::this_thread::sleep_for(1ms);
        }
        return done;
    } catch (exit_exception&) {
        return true;
    }
}

void Pet100::load(uint16_t start, uint8_t const* ptr, size_t size) const
{
    constexpr uint8_t Sys_Token = 0x9e;
    constexpr uint8_t Space = 0x20;

    if (start == 0x0801 || start == 0x0401) {
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
            fmt::print("Detected basic start at {:04x}\n", basicStart);
        }
    }
    mach.writeRam(start, ptr, size);
}

void Pet100::start(uint16_t pc)
{
    fmt::print("Start {:04x}\n", pc);
    if ((pc == 0x0801 || pc == 0x0401) && basicStart >= 0) {
        pc = basicStart;
    }

    fillOutside(0x01);

    mach.setPC(pc);
    // emu->setPC(pc);
    // mach.setReg();
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
