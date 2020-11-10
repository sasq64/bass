
#include <ansi/console.h>
#include <ansi/terminal.h>

#include <coreutils/file.h>

#include "emulator.h"
#include <fmt/format.h>

#include <array>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

struct TextEmu
{
    enum Regs
    {
        WinX,
        WinY,
        WinW,
        WinH,

        RealW,
        RealH,

        TextPtr,
        ColorPtr,

        CFillIn,
        CFillOut,

        Key,
        Joy,
        TimerLo,
        TimerHi,
        Flags,

    };
    sixfive::Machine<> emu;

    std::array<uint8_t, 128> regs;
    std::unique_ptr<bbs::Console> console;

    std::vector<uint8_t> textRam;
    std::vector<uint8_t> colorRam;

    std::array<uint8_t, 128> palette;

    void set_color(uint8_t col)
    {
        int f = (col & 0xf) * 3;
        int b = ((col >> 4) + 16) * 3;
        uint32_t fg =
            (palette[f] << 24) | (palette[f + 1] << 16) | (palette[f + 2] << 8);
        uint32_t bg =
            (palette[b] << 24) | (palette[b + 1] << 16) | (palette[b + 2] << 8);
        console->set_color(fg, bg);
    }

    void writeChar(uint16_t adr, uint8_t t)
    {
        auto offset = adr - (regs[TextPtr] * 256);
        textRam[offset] = t;
        std::string s(1, (char)t);
        console->set_xy(offset % regs[WinW] + regs[WinX],
                        offset / regs[WinW] + regs[WinY]);
        set_color(colorRam[offset]);
        console->put(s);
    }

    void writeColor(uint16_t adr, uint8_t c)
    {
        auto offset = adr - (regs[ColorPtr] * 256);
        colorRam[offset] = c;
        console->set_xy(offset % regs[WinW] + regs[WinX],
                        offset / regs[WinW] + regs[WinY]);
        set_color(c);

        auto t = textRam[offset];
        std::string s(1, (char)t);
        console->put(s);
    }

    void updateRegs()
    {

        textRam.resize(regs[WinW] * regs[WinH]);
        colorRam.resize(regs[WinW] * regs[WinH]);

        auto banks = (regs[WinW] * regs[WinH] + 255) / 256;
        emu.mapWriteCallback(regs[TextPtr], banks, this,
                             [](uint16_t adr, uint8_t v, void* data) {
                                 auto* emu = static_cast<TextEmu*>(data);
                                 emu->writeChar(adr, v);
                             });
        emu.mapWriteCallback(regs[ColorPtr], banks, this,
                             [](uint16_t adr, uint8_t v, void* data) {
                                 auto* emu = static_cast<TextEmu*>(data);
                                 emu->writeColor(adr, v);
                             });
    }
    void fillInside()
    {
        auto v = regs[CFillIn];
        for (int i = 0; i < regs[WinH] * regs[WinW]; i++) {
            colorRam[i] = v;
        }
    }

    void fillOutside()
    {

        auto cw = console->get_width();
        auto ch = console->get_height();
        set_color(regs[CFillOut]);
        for (size_t y = 0; y < ch; y++) {
            for (size_t x = 0; x < cw; x++) {
                if (x < regs[WinX] || x >= (regs[WinX] + regs[WinW]) ||
                    y < regs[WinY] || y >= (regs[WinY] + regs[WinH])) {
                    console->set_xy(x, y);
                }
            }
        }
    }

    TextEmu()
    {
        auto terminal = bbs::create_local_terminal();
        terminal->open();
        console = std::make_unique<bbs::Console>(std::move(terminal));

        auto cw = console->get_width();
        auto ch = console->get_height();

        emu.mapWriteCallback(0xd7, 1, this,
                             [](uint16_t adr, uint8_t v, void* data) {
                                 auto* emu = static_cast<TextEmu*>(data);
                                 if (adr < 0xd780) {
                                     auto r = adr & 0xff;
                                     emu->regs[r] = v;
                                     if (r == CFillOut) {
                                         emu->fillOutside();
                                     }
                                     emu->updateRegs();
                                 } else {
                                     emu->palette[adr - 0xd780] = v;
                                 }
                             });
        regs[WinH] = 25;
        regs[WinW] = 40;
        regs[WinX] = (cw - regs[WinW]) / 2;
        regs[WinY] = (ch - regs[WinH]) / 2;

        regs[TextPtr] = 0x04;
        regs[ColorPtr] = 0xd8;
        for (int i = 0; i < 16 * 6; i++) {
            palette[i] = i >= 16 * 3 ? 0x00 : 0xc0;
        }
        updateRegs();
        for (auto& c : textRam) {
            c = 0x20;
        }
        for (auto& c : colorRam) {
            c = 0;
        }
    }
};

int main(int argc, char** argv)
{
    TextEmu emu;

    utils::File f{argv[1]};

    auto start = f.read<uint16_t>();
    auto data = f.readAll();

    emu.emu.writeRam(start, data.data() + 2, data.size() - 2);

    emu.regs[TextEmu::CFillOut] = 0x11;
    emu.fillOutside();

    // LOGI("Run %04x %02x %02x", start, data[2], data[3]);
    emu.emu.setPC(start);
    emu.emu.run();

    emu.console->flush();

    return 0;
}
