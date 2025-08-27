/*
 * Copyright 2022 Rive
 */

#include "bench.hpp"

#include <chrono>
#include <string.h>

// Run a microbenchmark repeatedly for a few seconds and print the quickest
// time.
#ifdef __EMSCRIPTEN__
int rive_wasm_main(int argc, const char* const* argv)
#else
int main(int argc, const char** argv)
#endif
{
    using namespace std::chrono_literals;
    using clock = std::chrono::high_resolution_clock;

    // Default flags.
    clock::duration benchDuration = 5s;

    // Parse flags.
    const char* const* arg = argv + 1;
    const char* const* endarg = argv + argc;
    while (arg + 1 < endarg && *arg[0] == '-')
    {
        if (!strcmp(*arg, "--duration") || !strcmp(*arg, "-d"))
        {
            benchDuration = std::chrono::seconds(atoi(arg[1]));
        }
        arg += 2;
    }

    // Find the selected benchmark.
    Bench::BenchMap& registry = Bench::Registry();
    const char* benchName = arg < endarg ? *arg : "";
    auto benchIter = registry.find(benchName);
    if (benchIter == registry.end())
    {
        // Print out the usage unless they ran "bench list".
        if (arg >= endarg || !!strcmp(*arg, "list"))
        {
            fprintf(stderr,
                    "Usage:\n\nbench [--duration <seconds>] <benchmark>\n\n");
            fprintf(stderr, "Benchmarks:\n\n");
            fflush(stderr);
        }
        // Print out the benchmarks.
        for (const auto& [name, constructor] : registry)
        {
            printf("%s ", name.c_str());
        }
        printf("\n");
        fflush(stdout);
        return 1;
    }

    // Run the benchmark.
    auto constructor = benchIter->second;
    std::unique_ptr<Bench> bench(constructor());
    bench->setup();

    clock::duration minTime = clock::duration::max();
    clock::time_point quitTime = clock::now() + benchDuration;
    clock::time_point start, end;
    do
    {
        start = clock::now();
        bench->run();
        end = clock::now();
        minTime = std::min(end - start, minTime);
    } while (end < quitTime);

#ifdef DEBUG
    printf("<time hidden in debug builds> %s\n", benchName);
#else
    printf("%.4gms %s\n",
           std::chrono::nanoseconds(minTime).count() * 1e-6,
           benchName);
#endif

    return 0;
}
