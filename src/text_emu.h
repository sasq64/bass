#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace sixfive {
struct DefaultPolicy;
template <class POLICY>
struct Machine;
} // namespace sixfive

namespace bbs {
class Console;
} // namespace bbs

struct exit_exception : public std::exception
{};


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

        CFillOut,

        Keys,
        Freq,
        TimerLo,
        TimerHi,
        Control,
        Charset,
        IrqE,
        IrqR

    };
    std::unique_ptr<sixfive::Machine<sixfive::DefaultPolicy>> emu;
    std::array<uint8_t, 128> regs{};
    std::unique_ptr<bbs::Console> console;
    std::vector<uint8_t> textRam;
    std::vector<uint8_t> colorRam;
    std::array<uint8_t, 128> palette{};

    mutable int32_t basicStart = -1;

    void set_color(uint8_t col);
    void writeChar(uint16_t adr, uint8_t t);
    void writeColor(uint16_t adr, uint8_t c);
    void updateRegs();
    void fillOutside(uint8_t col);

    int get_width() const;
    int get_height() const;

    void doUpdate();

    uint8_t readReg(int reg);
    void writeReg(int reg, uint8_t val);
    uint16_t get_ticks() const;

    TextEmu();
    ~TextEmu();

    void run(uint16_t start);
    void load(uint16_t start, uint8_t const* ptr, size_t size) const;

    void load(uint16_t start, std::vector<uint8_t> const& data) const
    {
        load(start, data.data(), data.size());
    }
    void start(uint16_t pc);
    bool update();
    using clk = std::chrono::steady_clock;
    clk::time_point start_t;
    clk::time_point nextUpdate;
};

