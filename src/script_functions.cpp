#include "assembler.h"
#include "machine.h"
#include "script.h"

#include <sol/sol.hpp>

void registerLuaFunctions(Assembler& a, Scripting& s)
{
    auto& lua = s.getState();
    auto& mach = a.getMachine();

    lua["read_mem_6502"] = [&](int adr) { return mach.readRam(adr); };
    lua["mem_read"] = [&](int adr) { return mach.readRam(adr); };
    lua["mem_write"] = [&](int adr, int val) {
        return mach.writeRam(adr, val);
    };

    lua["set_break_fn"] = [&](int what, std::function<void(int)> const& fn) {
        mach.setBreakFunction(what, fn);
    };

    lua["map_bank_write"] = [&](int what, int len,
                                std::function<void(uint16_t, uint8_t)> const& fn) {
        mach.setBankWrite(what, len, fn);
    };
    lua["map_bank_read"] = [&](int what, int len, std::function<uint8_t(uint16_t)> const& fn) {
        mach.setBankRead(what, len, fn);
    };

    lua["reg_a"] = [&]() {
        auto const& [a,x,y,sr,sp,pc] = mach.getRegs();
        return a;
    };
    lua["reg_x"] = [&]() {
        auto const& [a,x,y,sr,sp,pc] = mach.getRegs();
        return x;
    };
    lua["reg_y"] = [&]() {
        auto const& [a,x,y,sr,sp,pc] = mach.getRegs();
        return y;
    };
    lua["set_x"] = [&](int x) {
        auto regs = mach.getRegs();
        std::get<1>(regs) = x;
        mach.setRegs(regs);
    };
    lua["set_y"] = [&](int y) {
        auto regs = mach.getRegs();
        std::get<2>(regs) = y;
        mach.setRegs(regs);
    };
}
