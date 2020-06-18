#include "script.h"
#include "assembler.h"
#include "machine.h"

#include <sol/sol.hpp>

void registerLuaFunctions(Assembler& a, Scripting& s)
{
    auto& lua = s.getState();
    auto& mach = a.getMachine();

    lua["read_mem_6502"] = [&](int adr) {
        return mach.readRam(adr);
    };

    lua["set_break_fn"] = [&](int what, std::function<void(int)> const& fn) {
        fn(what);
    };
}
