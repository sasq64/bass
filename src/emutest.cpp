#include "../external/coreutils/src/coreutils/file.h"
#include "emulator.h"
#include <benchmark/benchmark.h>

#include <cstdio>
#include <chrono>
#include <vector>

struct Result
{
    int calls;
    int opcodes;
    int jumps;
    bool tooLong;
};


struct DirectPolicy : sixfive::DefaultPolicy {
    DirectPolicy() = default;
	explicit DirectPolicy(sixfive::Machine<DirectPolicy>&m) {}
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






//static void Bench_full(benchmark::State& state) {
//    utils::File f{"6502test.bin"};
//    auto data = f.readAll();
//    data[0x3b91] = 0x60;
//    sixfive::Machine<DirectPolicy> m;
//    while (state.KeepRunning()) {
//        m.write_ram(0, data.data(), data.size());
//        m.setPC(0x1000);
//        m.run();
//    }
//    // 30036805
//}
//BENCHMARK(Bench_full);

template <typename Policy>
static void Sort(benchmark::State& state)
{
    static const uint8_t sortCode[] = {
        0xa0, 0x00, 0x84, 0x32, 0xb1, 0x30, 0xaa, 0xc8, 0xca, 0xb1,
        0x30, 0xc8, 0xd1, 0x30, 0x90, 0x10, 0xf0, 0x0e, 0x48, 0xb1,
        0x30, 0x88, 0x91, 0x30, 0x68, 0xc8, 0x91, 0x30, 0xa9, 0xff,
        0x85, 0x32, 0xca, 0xd0, 0xe6, 0x24, 0x32, 0x30, 0xd9, 0x60};

    static const uint8_t data[] = {
        0,   19, 73,  2,   54,  97,  21,  45,  66,  13, 139, 56,  220, 50,
        30,  20, 67,  111, 109, 175, 4,   66,  100, 19, 73,  2,   54,  97,
        21,  45, 66,  13,  139, 56,  220, 50,  30,  20, 67,  111, 109, 175,
        4,   66, 100, 19,  73,  2,   54,  97,  21,  45, 66,  13,  139, 56,
        220, 50, 30,  20,  67,  111, 109, 175, 4,   66, 100,
    };

    sixfive::Machine<Policy> m;
    for (int i = 0; i < std::ssize(data); i++)
        m.write_ram(0x2000 + i, data[i]);
    for (int i = 0; i < std::ssize(sortCode); i++)
        m.write_ram(0x1000 + i, sortCode[i]);
    m.write_ram(0x30, 0x00);
    m.write_ram(0x31, 0x20);
    m.write_ram(0x2000, std::size(data) - 1);
    m.setPC(0x1000);
    m.run();
    while (state.KeepRunning()) {
        for (int i = 1; i < (int)sizeof(data); i++)
            m.write_ram(0x2000 + i, data[i]);
        m.setPC(0x1000);
        m.run();
    }
}

static void Bench_sort(benchmark::State& state) {
    Sort<DirectPolicy>(state);
}
BENCHMARK(Bench_sort);
static void Bench_sort2(benchmark::State& state) {
    Sort<sixfive::DefaultPolicy>(state);
}
BENCHMARK(Bench_sort2);

static void Bench_dayofweek(benchmark::State& state)
{

    static const unsigned char WEEK[] = {
        0xa0, 0x74, 0xa2, 0x0a, 0xa9, 0x07, 0x20, 0x0a, 0x10, 0x60, 0xe0,
        0x03, 0xb0, 0x01, 0x88, 0x49, 0x7f, 0xc0, 0xc8, 0x7d, 0x2a, 0x10,
        0x85, 0x06, 0x98, 0x20, 0x26, 0x10, 0xe5, 0x06, 0x85, 0x06, 0x98,
        0x4a, 0x4a, 0x18, 0x65, 0x06, 0x69, 0x07, 0x90, 0xfc, 0x60, 0x01,
        0x05, 0x06, 0x03, 0x01, 0x05, 0x03, 0x00, 0x04, 0x02, 0x06, 0x04};

    sixfive::Machine<DirectPolicy> m;
    for (int i = 0; i < (int)sizeof(WEEK); i++)
        m.write_ram(0x1000 + i, WEEK[i]);
    m.setPC(0x1000);
    m.run(5000);
    while (state.KeepRunning()) {
        m.setPC(0x1000);
        m.run(5000);
    }
};

BENCHMARK(Bench_dayofweek);

static void Bench_allops(benchmark::State& state)
{
    sixfive::Machine<DirectPolicy> m;
    m.setPC(0x1000);
    auto instr = m.getInstructions();
    ssize_t total;
    while (state.KeepRunning()) {
        total = 0;
        m.setPC(0x1000);
        for (const auto& i : instr) {
            total += std::ssize(i.opcodes);
            for (const auto& o : i.opcodes) {
                o.op(m);
            }
        }
    }
    //printf("Opcodes %zd\n", total);
}
BENCHMARK(Bench_allops);

