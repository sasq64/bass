#pragma once

#include "emulator.h"
#include "defines.h"

struct EmuPolicy : public sixfive::DefaultPolicy
{
    explicit EmuPolicy(sixfive::Machine<EmuPolicy>& m) : machine(m) {}

    static constexpr bool ExitOnStackWrap = true;

    // PC accesses does not normally need to work in IO areas
    static constexpr int PC_AccessMode = sixfive::Banked;

    // Generic reads and writes should normally not be direct
    static constexpr int Read_AccessMode = sixfive::Callback;
    static constexpr int Write_AccessMode = sixfive::Callback;

    static constexpr int MemSize = 65536;

    std::array<Intercept*, 64 * 1024> intercepts{};

    sixfive::Machine<EmuPolicy>& machine;

    // This function is run after each opcode. Return true to stop emulation.
    static bool eachOp(EmuPolicy& policy)
    {
        static FILE *fp = fopen("trace.log", "w");
        static unsigned last_pc = 0xffffffff;
        auto pc = policy.machine.regPC();

        if (pc != last_pc) {
            fprintf(fp, "%04x\n", pc);
            fflush(fp);
            //fmt::print("{:04x}\n", pc);
            if (auto* ptr = policy.intercepts[pc]) {
                return ptr->fn(pc);
            }
            last_pc = pc;
        }
        return false;
    }
};
