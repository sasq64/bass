#include "text_emu.h"

#include <ansi/console.h>
#include <ansi/terminal.h>

#include "emulator.h"

#include <array>
#include <chrono>

using namespace std::chrono_literals;

static constexpr std::array<uint8_t, 16 * 3> c64pal = {
    0xFF, 0xFF, 0xFF, // WHITE
    0x68, 0x37, 0x2B, // RED
    0x58, 0x8D, 0x43, // GREEN
    0x35, 0x28, 0x79, // BLUE
    0x6F, 0x4F, 0x25, // ORANGE
    0x00, 0x00, 0x00, // BLACK
    0x43, 0x39, 0x00, // BROWN
    0x9A, 0x67, 0x59, // LIGHT_READ
    0x44, 0x44, 0x44, // DARK_GREY
    0x6C, 0x6C, 0x6C, // GREY
    0x9A, 0xD2, 0x84, // LIGHT_GREEN
    0x6C, 0x5E, 0xB5, // LIGHT_BLUE
    0x95, 0x95, 0x95, // LIGHT_GREY
    0x6F, 0x3D, 0x86, // PURPLE
    0xB8, 0xC7, 0x6F, // YELLOW
    0x70, 0xA4, 0xB2, // CYAN
};

void TextEmu::set_color(uint8_t col)
{
    int f = (col & 0xf) * 3;
    int b = ((col >> 4) + 16) * 3;
    uint32_t fg =
        (palette[f] << 24) | (palette[f + 1] << 16) | (palette[f + 2] << 8);
    uint32_t bg =
        (palette[b] << 24) | (palette[b + 1] << 16) | (palette[b + 2] << 8);
    console->set_color(fg, bg);
}

void TextEmu::writeChar(uint16_t adr, uint8_t t)
{
    auto offset = adr - (regs[TextPtr] * 256);
    textRam[offset] = t;
    std::string s(1, static_cast<char>(t));
    console->set_xy(offset % regs[WinW] + regs[WinX],
                    offset / regs[WinW] + regs[WinY]);
    set_color(colorRam[offset]);
    console->put(s);
}

void TextEmu::writeColor(uint16_t adr, uint8_t c)
{
    auto offset = adr - (regs[ColorPtr] * 256);
    colorRam[offset] = c;
    console->set_xy(offset % regs[WinW] + regs[WinX],
                    offset / regs[WinW] + regs[WinY]);
    set_color(c);

    auto t = textRam[offset];
    std::string s(1, static_cast<char>(t));
    console->put(s);
}

void TextEmu::updateRegs()
{

    textRam.resize(regs[WinW] * regs[WinH]);
    colorRam.resize(regs[WinW] * regs[WinH]);

    auto banks = (regs[WinW] * regs[WinH] + 255) / 256;
    emu->mapWriteCallback(regs[TextPtr], banks, this,
                          [](uint16_t adr, uint8_t v, void* data) {
                              auto* thiz = static_cast<TextEmu*>(data);
                              thiz->writeChar(adr, v);
                          });
    emu->mapWriteCallback(regs[ColorPtr], banks, this,
                          [](uint16_t adr, uint8_t v, void* data) {
                              auto* thiz = static_cast<TextEmu*>(data);
                              thiz->writeColor(adr, v);
                          });
}
void TextEmu::fillInside()
{
    auto cw = console->get_width();
    auto ch = console->get_height();
    auto v = regs[CFillIn];
    set_color(v);
    size_t i = 0;
    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            if (x >= regs[WinX] && x < (regs[WinX] + regs[WinW]) &&
                y >= regs[WinY] && y < (regs[WinY] + regs[WinH])) {
                auto t = textRam[i];
                colorRam[i++] = v;
                console->set_xy(x, y);
                console->put(std::string(1, static_cast<char>(t)));
            }
        }
    }
}

void TextEmu::fillOutside()
{
    auto cw = console->get_width();
    auto ch = console->get_height();
    set_color(regs[CFillOut]);
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

TextEmu::TextEmu()
{
    auto terminal = bbs::create_local_terminal();
    terminal->open();
    console = std::make_unique<bbs::Console>(std::move(terminal));

    auto cw = console->get_width();
    auto ch = console->get_height();

    emu = std::make_unique<sixfive::Machine<>>();

    emu->mapReadCallback(0xd7, 1, this,
                         [](uint16_t adr, void* data) -> uint8_t {
                             auto* thiz = static_cast<TextEmu*>(data);
                             if (adr >= 0xd780) {
                                 return thiz->palette[adr - 0xd780];
                             }
                             auto r = adr & 0xff;
                             if (r == Key) {
                                 if (thiz->keys.empty()) {
                                     return 0;
                                 }
                                 auto k = thiz->keys.front();
                                 thiz->keys.pop_front();
                                 return k;
                             }
                             return thiz->regs[r];
                         });

    emu->mapWriteCallback(0xd7, 1, this,
                          [](uint16_t adr, uint8_t v, void* data) {
                              auto* thiz = static_cast<TextEmu*>(data);
                              if (adr < 0xd780) {
                                  auto r = adr & 0xff;
                                  thiz->regs[r] = v;
                                  if (r == CFillOut) {
                                      thiz->fillOutside();
                                  } else if (r == CFillIn) {
                                      thiz->fillInside();
                                  }
                                  thiz->updateRegs();
                              } else {
                                  thiz->palette[adr - 0xd780] = v;
                              }
                          });
    regs[WinH] = 25;
    regs[WinW] = 40;
    regs[WinX] = (cw - regs[WinW]) / 2;
    regs[WinY] = (ch - regs[WinH]) / 2;

    regs[TextPtr] = 0x04;
    regs[ColorPtr] = 0xd8;

    for (int i = 0; i < 16 * 3; i++) {
        palette[i] = c64pal[i];
        palette[i + 16 * 3] = c64pal[i];
    }
    updateRegs();
    for (auto& c : textRam) {
        c = 0x20;
    }
    for (auto& c : colorRam) {
        c = 0xca;
    }
}

void TextEmu::run(uint16_t start)
{
    regs[TextEmu::CFillOut] = 0x11;
    fillOutside();

    // LOGI("Run %04x %02x %02x", start, data[2], data[3]);
    emu->setPC(start);

    int cycles = 0;

    using clk = std::chrono::steady_clock;

    auto start_t = clk::now();

    while (cycles == 0) {
        cycles = emu->run(100000);
        auto key = console->read_key();
        if (key != 0) {
            // LOGI("KEY %x", key);
            keys.push_back(key);
        }
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            clk::now() - start_t);

        auto frames = ms.count() / 50;

        regs[TextEmu::TimerLo] = frames & 0xff;
        regs[TextEmu::TimerHi] = (frames >> 8) & 0xff;

        console->flush();
    }
}

void TextEmu::load(uint16_t start, uint8_t const* ptr, size_t size) const
{
    emu->writeRam(start, ptr, size);
}

TextEmu::~TextEmu() = default;
