#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

class Machine;

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


struct Pet100
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

        Border,
        Keys,
        Control,
        Charset,

        TimerLo,
        TimerMid,
        TimerHi,
        TimerDiv,

        IrqE,
        IrqR

    };
//    std::unique_ptr<sixfive::Machine<sixfive::DefaultPolicy>> emu;
    Machine& mach;
    std::array<uint8_t, 128> regs{};
    std::unique_ptr<bbs::Console> console;
    std::vector<uint8_t> textRam;
    std::vector<uint8_t> colorRam;
    std::array<uint8_t, 128> palette{};

    mutable int32_t basicStart = -1;

    void freeze_timer(bool freeze);

    void set_color(uint8_t col);
    void writeChar(uint16_t adr, uint8_t t);
    uint8_t readChar(uint16_t adr);
    void writeColor(uint16_t adr, uint8_t c);
    uint8_t readColor(uint16_t adr);
    void updateRegs();
    void fillOutside(uint8_t col);

    int get_width() const;
    int get_height() const;

    void doUpdate();

    uint8_t readReg(int reg);
    void writeReg(int reg, uint8_t val);
    uint32_t get_ticks() const;

    Pet100(Machine& m, bool noScreen);
    ~Pet100();

    void run(uint16_t start);
    void load(uint16_t start, uint8_t const* ptr, size_t size) const;

    void load(uint16_t start, std::vector<uint8_t> const& data) const
    {
        load(start, data.data(), data.size());
    }
    void start(uint16_t pc);
    bool update();
    bool frozenTimer = false;

    using clk = std::chrono::steady_clock;
    
    clk::duration frozenTime;

    clk::time_point start_t;
    clk::time_point nextUpdate;

    std::string fileName;
};

