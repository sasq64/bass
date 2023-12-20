#include <benchmark/benchmark.h>

void FullTest();

int main()
{
    //FullTest();
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}