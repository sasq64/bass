
#include "../external/coreutils/src/coreutils/file.h"
#include "emulator.h"

#include <cstdio>
#include <chrono>
#include <vector>

struct DirectPolicy : sixfive::DefaultPolicy {
    DirectPolicy() = default;
    explicit DirectPolicy(sixfive::Machine<DirectPolicy>&m) {}
    static constexpr bool UseSwitch = true;
    static constexpr int PC_AccessMode = sixfive::Direct;
    static constexpr int Read_AccessMode = sixfive::Direct;
    static constexpr int Write_AccessMode = sixfive::Direct;
};

struct CheckPolicy : public DirectPolicy {

    sixfive::Machine<CheckPolicy>& machine;

    explicit CheckPolicy(sixfive::Machine<CheckPolicy>& m) : machine(m) {}

    int ops = 0;
    static bool eachOp(CheckPolicy& dp)
    {
        auto& m = dp.machine;
        static int lastpc = -1;
        const auto [a, x, y, sr, sp, pc] = m.regs();
        if (m.regPC() == lastpc) {
            printf("STALL @ %04x A %02x X %02x Y %02x SR %02x SP %02x\n",
                   lastpc, a, x, y, sr, sp);
            for (int i = 0; i < 256; i++)
                printf("%02x ", m.get_stack(i));
            printf("\n");

            return true;
        }
        lastpc = m.regPC();
        //printf("%04x : %02x %02x %02x : %02x -- %d\n", lastpc, a, x, y, sr, m.cycles);
        dp.ops++;
        return false;
    }

    static inline bool doTrace = false;
};


struct OpCountPolicy : DirectPolicy {
    explicit OpCountPolicy(sixfive::Machine<OpCountPolicy>&m) {}
    int ops = 0;

    static constexpr bool eachOp(OpCountPolicy& p) {
        p.ops++;
        return false;
    }
    static constexpr void afterRun(OpCountPolicy& p) {
        printf("Opcodes: %d\n",p.ops);
    }
};

template <typename Policy> uint32_t
RunTest(sixfive::Machine<Policy>& m, std::vector<uint8_t> const& data, int count)
{
    uint32_t cycles;
    for (int i=0; i< count; i++) {
        m.write_ram(0, data.data(), data.size());
        m.setPC(0x1000);
        cycles = m.run();
    }
    return cycles;
}

void FullTest()
{
    utils::File f{"6502test.bin"};
    auto data = f.readAll();
    // PC: 3B91 indicates successful test, so return
    data[0x3b91] = 0x60;

    printf("Running full test\n");
    sixfive::Machine<CheckPolicy> m0;
    auto cycles = RunTest(m0, data, 1);

    auto opcodes = m0.policy().ops;
    printf("Opcodes: %d\n", opcodes);
    printf("Cycles: %d\n", cycles);

    sixfive::Machine<DirectPolicy> m1;

    auto start = std::chrono::system_clock::now();
    cycles = RunTest(m1, data, 10);
    auto stop = std::chrono::system_clock::now();

    auto d = (stop - start);
    auto us =
        std::chrono::duration_cast<std::chrono::microseconds>(d).count();

    double no = us * 100.0 / opcodes;

    double nc = us * 100.0 / cycles; // time for one cycle

    auto total = (int)(1'000'000'000 / no);
    auto mhz = 1000.0 / nc;
    printf("Total %lld us\n%f ns per opcode\n%d opcodes per sec\n%f MHz", us/10, no, total, mhz);
}


int main()
{
    FullTest();
    return 0;
}