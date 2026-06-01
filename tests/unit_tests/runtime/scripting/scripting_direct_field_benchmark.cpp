#include "catch.hpp"
#include "lua.h"
#include "lualib.h"
#include "Luau/Common.h"
#include "scripting_test_utilities.hpp"
#include <chrono>
#include <climits>

LUAU_FASTFLAG(LuauDirectFieldGet)

using namespace rive;

namespace
{
struct ScopedFFlag
{
    Luau::FValue<bool>& flag;
    const bool saved;
    ScopedFFlag(Luau::FValue<bool>& f, bool newValue) : flag(f), saved(f.value)
    {
        flag.value = newValue;
    }
    ~ScopedFFlag() { flag.value = saved; }
};

struct BenchResult
{
    const char* name;
    long long offUs;
    long long onUs;
};

long long bestRun(const char* source)
{
    const int WARMUP = 3;
    const int RUNS = 5;
    long long best = LLONG_MAX;
    for (int run = 0; run < WARMUP + RUNS; ++run)
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        ScriptingTest test(source);
        auto t1 = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
                      .count();
        if (run >= WARMUP && us < best)
            best = us;
    }
    return best;
}
} // namespace

TEST_CASE("userdata direct field get benchmark", "[scripting][benchmark]")
{
    const int N = 1000000;
    char script[2048];

    auto benchOp = [&](const char* name, const char* body) -> BenchResult {
        long long off;
        long long on;
        {
            ScopedFFlag guard(FFlag::LuauDirectFieldGet, false);
            off = bestRun(body);
        }
        {
            ScopedFFlag guard(FFlag::LuauDirectFieldGet, true);
            on = bestRun(body);
        }
        return {name, off, on};
    };

    snprintf(script,
             sizeof(script),
             "local p = Paint.new()\n"
             "p.thickness = 2\n"
             "local s = 0\n"
             "for i = 1, %d do s = s + p.thickness end\n"
             "return s\n",
             N);
    auto thicknessR = benchOp("Paint.thickness", script);

    snprintf(script,
             sizeof(script),
             "local p = Paint.new()\n"
             "p.color = 0xFFFFFFFF\n"
             "local s = 0\n"
             "for i = 1, %d do s = s + p.color end\n"
             "return s\n",
             N);
    auto colorR = benchOp("Paint.color", script);

    // Mat2D — 2-char alphabetic field name (xx). Existing slow path uses a
    // nested switch on namelen / first char rather than atom dispatch.
    snprintf(script,
             sizeof(script),
             "local m = Mat2D.values(1, 2, 3, 4, 5, 6)\n"
             "local s = 0\n"
             "for i = 1, %d do s = s + m.xx + m.ty end\n"
             "return s\n",
             N);
    auto mat2dR = benchOp("Mat2D.xx + .ty", script);

    // Mat4 — the slow path runs strtol() on the field name on every read.
    // Expect the biggest speedup here.
    snprintf(script,
             sizeof(script),
             "local m = Mat4.identity()\n"
             "local s = 0\n"
             "for i = 1, %d do s = s + m.m11 + m.m22 + m.m33 + m.m44 end\n"
             "return s\n",
             N);
    auto mat4R = benchOp("Mat4.m11..m44", script);

    BenchResult results[] = {thicknessR, colorR, mat2dR, mat4R};

    fprintf(stderr,
            "\n=== Userdata Direct Field Get Benchmark (%d iters, best of 5 "
            "runs) ===\n",
            N);
    fprintf(stderr,
            "  %-20s %12s %12s %10s\n",
            "Field",
            "FFlag OFF",
            "FFlag ON",
            "Speedup");
    fprintf(stderr, "  %-20s %12s %12s %10s\n", "", "(us)", "(us)", "");
    for (auto& r : results)
    {
        fprintf(stderr,
                "  %-20s %12lld %12lld %9.2fx\n",
                r.name,
                r.offUs,
                r.onUs,
                (double)r.offUs / (double)r.onUs);
    }
    fprintf(stderr, "  ===========================================\n");

    for (auto& r : results)
    {
        INFO("field=" << r.name << " off=" << r.offUs << "us on=" << r.onUs
                      << "us");
        CHECK(r.onUs < r.offUs);
    }
}
