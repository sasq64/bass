#include "emulator.h"
#include <benchmark/benchmark.h>

//#include <coreutils/format.h>

#include <cstdio>
#include <vector>
#include <string>

namespace sixfive {

struct Result
{
    int calls;
    int opcodes;
    int jumps;
    bool tooLong;
};


struct DirectPolicy : sixfive::DefaultPolicy {
	explicit DirectPolicy(sixfive::Machine<DirectPolicy>&m) {}
    int ops = 0;

    static constexpr bool eachOp(DirectPolicy& p) {
        p.ops++;
        return false;
    }

    static constexpr int PC_AccessMode = Direct;
    static constexpr int Read_AccessMode = Direct;
    static constexpr int Write_AccessMode = Direct;
};

static void Bench_sort(benchmark::State& state)
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

    sixfive::Machine<DirectPolicy> m;
    for (int i = 0; i < (int)sizeof(data); i++)
        m.write_ram(0x2000 + i, data[i]);
    for (int i = 0; i < (int)sizeof(sortCode); i++)
        m.write_ram(0x1000 + i, sortCode[i]);
    m.write_ram(0x30, 0x00);
    m.write_ram(0x31, 0x20);
    m.write_ram(0x2000, sizeof(data) - 1);
    m.setPC(0x1000);
    m.run(500000);
    printf("Opcodes %d\n", m.policy().ops);
    while (state.KeepRunning()) {
        for (int i = 1; i < (int)sizeof(data); i++)
            m.write_ram(0x2000 + i, data[i]);
        m.setPC(0x1000);
        m.run(500000);
    }
}
BENCHMARK(Bench_sort);

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
    printf("Opcodes %d\n", m.policy().ops);
    while (state.KeepRunning()) {
        m.setPC(0x1000);
        m.run(5000);
    }
};

BENCHMARK(Bench_dayofweek);

static void Bench_allops(benchmark::State& state)
{
    sixfive::Machine<> m;
    m.setPC(0x1000);
    auto instr = m.getInstructions();
    int total;
    while (state.KeepRunning()) {
        total = 0;
        m.setPC(0x1000);
        for (const auto& i : instr) {
            total += i.opcodes.size();
            for (const auto& o : i.opcodes) {
                o.op(m);
            }
        }
    }
    printf("Opcodes %d\n", total);
}
BENCHMARK(Bench_allops);

} // namespace sixfive
