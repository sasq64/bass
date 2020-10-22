#include "assembler.h"
#include "machine.h"
#include "script.h"
#include <fmt/core.h>
#include <fmt/format.h>

#include <sol/sol.hpp>

void registerLuaFunctions(Assembler& a, Scripting& s)
{
    auto& lua = s.getState();
    auto& mach = a.getMachine();

    lua["fmt"] = [&](std::string const& f, sol::variadic_args args) {
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        for (auto arg : args) {
            if (arg.is<int32_t>()) {
                store.push_back(arg.as<int32_t>());
            } else 
            if (arg.is<std::string>()) {
                store.push_back(arg.as<std::string>());
            }
        }
        std::string result = fmt::vformat(f, store);
        puts(result.c_str());
    };

    lua["sym"] = [&](std::string const& name) {
        auto aval = a.getSymbols().get(name);
        return s.to_object(aval);
    };

    lua["start_run"] = [&] {
        mach.runSetup();
    };

    lua["call"] = [&](int32_t adr) { mach.go(adr); };

    lua["read_mem_6502"] = [&](int adr) { return mach.readRam(adr); };
    lua["mem_read"] = [&](int adr) { return mach.readRam(adr); };
    lua["mem_write"] = [&](int adr, int val) {
        return mach.writeRam(adr, val);
    };

    //    lua["set_break_fn"] = [&](int what, std::function<void(int)> const&
    //    fn) {
    //        mach.setBreakFunction(what, fn);
    //    };

    lua["map_bank_write"] =
        [&](int hi_adr, int len,
            std::function<void(uint16_t, uint8_t)> const& fn) {
            mach.setBankWrite(hi_adr, len, fn);
        };
    lua["map_bank_read"] = sol::overload(
        [&](int hi_adr, int len, std::function<uint8_t(uint16_t)> const& fn) {
            mach.setBankRead(hi_adr, len, fn);
        },
        [&](int hi_adr, int len, int bank) {
            mach.setBankRead(hi_adr, len, bank);
        });

    lua["reg_a"] = [&]() { return mach.getReg(sixfive::Reg::A); };
    lua["reg_x"] = [&]() { return mach.getReg(sixfive::Reg::X); };
    lua["reg_y"] = [&]() { return mach.getReg(sixfive::Reg::Y); };
    lua["set_a"] = [&](int a) { mach.setReg(sixfive::Reg::A, a); };
    lua["set_x"] = [&](int x) { mach.setReg(sixfive::Reg::X, x); };
    lua["set_y"] = [&](int y) { mach.setReg(sixfive::Reg::Y, y); };
}
