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
                                std::function<void(int, int)> const& fn) {
        mach.setBankWrite(what, len, fn);
        fn(what, 0);
    };
    lua["map_bank_read"] = [&](int what, int len, std::function<int(int)> const& fn) {
        fn(what);
        mach.setBankRead(what, len, fn);
    };
}
