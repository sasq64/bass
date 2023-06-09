#include "assembler.h"
#include "keycodes.h"
#include "machine.h"
#include "script.h"

#include <fmt/core.h>

#include <sol/sol.hpp>

void registerLuaFunctions(Assembler& assembler, Scripting& scripting)
{
    auto& lua = scripting.getState();
    auto& mach = assembler.getMachine();

    lua.new_usertype<Assembler::Block>("MetaBlock", "contents",
                                       &Assembler::Block::contents, "line",
                                       &Assembler::Block::line);

    lua.new_usertype<Assembler::Meta>(
        "Meta", "text", &Assembler::Meta::text, "name", &Assembler::Meta::name,
        "args", &Assembler::Meta::vargs, "line", &Assembler::Meta::line,
        "blocks", &Assembler::Meta::blocks);

    lua["KEY_UP"] = KEY_UP;
    lua["KEY_DOWN"] = KEY_DOWN;
    lua["KEY_LEFT"] = KEY_LEFT;
    lua["KEY_RIGHT"] = KEY_RIGHT;
    lua["KEY_F1"] = KEY_F1;
    lua["KEY_F3"] = KEY_F3;
    lua["KEY_F5"] = KEY_F5;
    lua["KEY_F7"] = KEY_F7;

    lua["assemble_block"] = [&](Assembler::Block const& block) {
        assembler.evaluateBlock(block);
    };
    lua["assemble"] = [&](std::string const& source) {
      assembler.evaluateCode(source, "from_lua");
    };

    lua["get_meta_fn"] = [&](std::string const& name) {
        return assembler.getMetaFn(name);
    };

    lua["register_meta"] = [&](std::string const& name,
                               Assembler::MetaFn const& meta) {
        assembler.registerMeta(name, meta);
    };

    lua["fmt"] = [&](std::string const& text, sol::variadic_args args) {
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        for (auto arg : args) {
            if (arg.is<int32_t>()) {
                store.push_back(arg.as<int32_t>());
            } else if (arg.is<std::string>()) {
                store.push_back(arg.as<std::string>());
            }
        }
        auto result = fmt::vformat(text, store);
        puts(result.c_str());
    };

    lua["sym"] = [&](std::string const& name) {
        auto aval = assembler.getSymbols().get(name);
        return scripting.to_object(aval);
    };

    lua["start_run"] = [&] { mach.runSetup(); };

    lua["call"] = [&](int32_t adr) { mach.go(adr); };

    lua["read_mem_6502"] = [&](int adr) { return mach.readRam(adr); };
    lua["mem_read"] = [&](int adr) { return mach.readMem(adr); };
    lua["mem_write"] = [&](int adr, int val) {
        return mach.writeRam(adr, val);
    };

    lua["cbm_load"] = [&](std::string const& name) {
        const utils::File f{name};
        uint16_t start = f.read<uint8_t>();
        start |= (f.read<uint8_t>() << 8);
        // fmt::print("Loading {} to {:04x}\n", name, start);
        std::vector<uint8_t> data(f.getSize() - 2);
        f.read(data.data(), data.size());
        mach.writeRam(start, data.data(), data.size());
    };

    //    lua["set_break_fn"] = [&](int what, std::function<void(int)> const&
    //    fn) {
    //        mach.setBreakFunction(what, fn);
    //    };

    lua["map_jsr"] = [&](uint16_t adr,
                         std::function<void(uint16_t)> const& fn) {
        mach.addJsrFunction(adr, fn);
    };

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

    lua["reg_pc"] = [&]() { return mach.getReg(sixfive::Reg::PC); };
    lua["reg_a"] = [&]() { return mach.getReg(sixfive::Reg::A); };
    lua["reg_x"] = [&]() { return mach.getReg(sixfive::Reg::X); };
    lua["reg_y"] = [&]() { return mach.getReg(sixfive::Reg::Y); };
    lua["set_a"] = [&](int a) { mach.setReg(sixfive::Reg::A, a); };
    lua["set_x"] = [&](int x) { mach.setReg(sixfive::Reg::X, x); };
    lua["set_y"] = [&](int y) { mach.setReg(sixfive::Reg::Y, y); };
}
