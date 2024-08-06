#include <benchmark/benchmark.h>

extern "C" {

#include "util.h"

}

static void
BM_colwidth(benchmark::State& state)
{
    const char str[] = "\t\t"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    for (auto _ : state)
        colwidth(str, sizeof(str) - 1, 1);
}
BENCHMARK(BM_colwidth);

BENCHMARK_MAIN();
