#include <benchmark/benchmark.h>

int main()
{
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}