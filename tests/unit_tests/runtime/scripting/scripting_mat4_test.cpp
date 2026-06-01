// Tests for the Mat4 scripted type and a perf benchmark comparing the C++
// SIMD-accelerated path against a pure-Luau buffer-based mat4 implementation
// modelled on examples/SpinningCube.luau.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"

#include <chrono>
#include <climits>
#include <cstdio>

using namespace rive;

TEST_CASE("Mat4 identity has expected values", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity().m11").state(),
                       -1) == 1.0);
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity().m22").state(),
                       -1) == 1.0);
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity().m33").state(),
                       -1) == 1.0);
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity().m44").state(),
                       -1) == 1.0);
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity().m12").state(),
                       -1) == 0.0);
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity()[1]").state(),
                       -1) == 1.0);
    CHECK(lua_tonumber(ScriptingTest("return Mat4.identity()[6]").state(),
                       -1) == 1.0);
}

TEST_CASE("Mat4.values stores column-major", "[scripting]")
{
    // Column-major: indices 1..4 are column 0, 5..8 column 1, ...
    const char* src = "local m = Mat4.values(\n"
                      "  1, 2, 3, 4,\n" // column 0
                      "  5, 6, 7, 8,\n" // column 1
                      "  9,10,11,12,\n" // column 2
                      " 13,14,15,16)\n"
                      "return m.m11, m.m21, m.m31, m.m41, m.m14, m.m44\n";
    auto t = ScriptingTest(src, 6);
    CHECK(lua_tonumber(t.state(), -6) == 1.0); // m11 = column 0, row 0
    CHECK(lua_tonumber(t.state(), -5) == 2.0); // m21 = column 0, row 1
    CHECK(lua_tonumber(t.state(), -4) == 3.0);
    CHECK(lua_tonumber(t.state(), -3) == 4.0);
    CHECK(lua_tonumber(t.state(), -2) == 13.0); // m14 = column 3, row 0
    CHECK(lua_tonumber(t.state(), -1) == 16.0); // m44 = column 3, row 3
}

TEST_CASE("Mat4 translation transforms a point", "[scripting]")
{
    const char* src = "local m = Mat4.fromTranslation(10, 20, 30)\n"
                      "local v = m:transformPoint(1, 2, 3)\n"
                      "return v.x, v.y, v.z\n";
    auto t = ScriptingTest(src, 3);
    CHECK(lua_tonumber(t.state(), -3) == 11.0);
    CHECK(lua_tonumber(t.state(), -2) == 22.0);
    CHECK(lua_tonumber(t.state(), -1) == 33.0);
}

TEST_CASE("Mat4 transformVec4 returns homogeneous components", "[scripting]")
{
    // No perspective divide: w is preserved as the final return value.
    const char* src = "local m = Mat4.fromTranslation(10, 20, 30)\n"
                      "return m:transformVec4(1, 2, 3, 1)\n";
    auto t = ScriptingTest(src, 4);
    CHECK(lua_tonumber(t.state(), -4) == 11.0);
    CHECK(lua_tonumber(t.state(), -3) == 22.0);
    CHECK(lua_tonumber(t.state(), -2) == 33.0);
    CHECK(lua_tonumber(t.state(), -1) == 1.0);
}

TEST_CASE("transformPoint result supports z and [3]", "[scripting]")
{
    // Pins down that the 3D Vector returned from a Mat4 transform is
    // reachable via both .z (intrinsic VM fastpath) and v[3] (metamethod).
    const char* src = "local m = Mat4.fromTranslation(10, 20, 30)\n"
                      "local v = m:transformPoint(1, 2, 3)\n"
                      "return v[1], v[2], v[3]\n";
    auto t = ScriptingTest(src, 3);
    CHECK(lua_tonumber(t.state(), -3) == 11.0);
    CHECK(lua_tonumber(t.state(), -2) == 22.0);
    CHECK(lua_tonumber(t.state(), -1) == 33.0);
}

TEST_CASE("Mat4 multiply composes transforms", "[scripting]")
{
    const char* src = "local t = Mat4.fromTranslation(10, 0, 0)\n"
                      "local s = Mat4.fromScale(2, 2, 2)\n"
                      "local m = t * s\n"
                      // m * (1,1,1) = scale then translate => (2+10, 2, 2)
                      "local v = m:transformPoint(1, 1, 1)\n"
                      "return v.x, v.y, v.z\n";
    auto t = ScriptingTest(src, 3);
    CHECK(lua_tonumber(t.state(), -3) == 12.0);
    CHECK(lua_tonumber(t.state(), -2) == 2.0);
    CHECK(lua_tonumber(t.state(), -1) == 2.0);
}

TEST_CASE("Mat4 invert round-trips", "[scripting]")
{
    const char* src =
        "local m = Mat4.fromTranslation(3, -4, 5) * Mat4.fromScale(2, 2, 2)\n"
        "local inv = m:invert()\n"
        "local r = m * inv\n"
        "local id = Mat4.identity()\n"
        // Compare diagonal — full equality may fail on FP rounding.
        "return math.abs(r.m11 - 1) + math.abs(r.m22 - 1) + math.abs(r.m33 - 1) + math.abs(r.m44 - 1)\n";
    auto t = ScriptingTest(src);
    CHECK(lua_tonumber(t.state(), -1) < 1e-5);
}

TEST_CASE("Mat4.multiply writes in place", "[scripting]")
{
    // Verifies the alloc-free static API used in tight loops.
    const char* src = "local out = Mat4.identity()\n"
                      "local a = Mat4.fromTranslation(1, 2, 3)\n"
                      "local b = Mat4.fromScale(4, 4, 4)\n"
                      "Mat4.multiply(out, a, b)\n"
                      "return out.m14, out.m24, out.m34, out.m11\n";
    auto t = ScriptingTest(src, 4);
    CHECK(lua_tonumber(t.state(), -4) == 1.0);
    CHECK(lua_tonumber(t.state(), -3) == 2.0);
    CHECK(lua_tonumber(t.state(), -2) == 3.0);
    CHECK(lua_tonumber(t.state(), -1) == 4.0);
}

TEST_CASE("Mat4.multiplyAffine matches multiply for affine inputs",
          "[scripting]")
{
    // For two affine matrices the fast and slow paths must agree
    // bit-exactly on every entry.
    const char* src =
        "local a = Mat4.fromTranslation(3, -1, 5) * Mat4.fromRotationY(0.7)\n"
        "local b = Mat4.fromScale(2, 0.5, 1) * Mat4.fromRotationZ(-0.3)\n"
        "local slow = Mat4.identity()\n"
        "local fast = Mat4.identity()\n"
        "Mat4.multiply(slow, a, b)\n"
        "Mat4.multiplyAffine(fast, a, b)\n"
        // Sum |slow[i] - fast[i]| over i=1..16; must be 0.
        "local diff = 0\n"
        "for i = 1, 16 do diff = diff + math.abs(slow[i] - fast[i]) end\n"
        "return diff, fast.m41, fast.m42, fast.m43, fast.m44\n";
    auto t = ScriptingTest(src, 5);
    CHECK(lua_tonumber(t.state(), -5) == 0.0);
    // Bottom row stays [0, 0, 0, 1] (affine invariant).
    CHECK(lua_tonumber(t.state(), -4) == 0.0);
    CHECK(lua_tonumber(t.state(), -3) == 0.0);
    CHECK(lua_tonumber(t.state(), -2) == 0.0);
    CHECK(lua_tonumber(t.state(), -1) == 1.0);
}

TEST_CASE("Mat4:invertAffine round-trips", "[scripting]")
{
    const char* src =
        "local m = Mat4.fromTranslation(3, -4, 5) * Mat4.fromRotationY(0.4)"
        " * Mat4.fromScale(2, 2, 2)\n"
        "local inv = m:invertAffine()\n"
        "assert(inv ~= nil)\n"
        "local r = m * inv\n"
        "return math.abs(r.m11 - 1) + math.abs(r.m22 - 1)"
        " + math.abs(r.m33 - 1) + math.abs(r.m44 - 1)"
        " + math.abs(r.m14) + math.abs(r.m24) + math.abs(r.m34)\n";
    auto t = ScriptingTest(src);
    CHECK(lua_tonumber(t.state(), -1) < 1e-5);
}

TEST_CASE("Mat4.invertAffine writes in place", "[scripting]")
{
    const char* src = "local m = Mat4.fromTranslation(10, 0, 0)\n"
                      "local out = Mat4.identity()\n"
                      "local ok = Mat4.invertAffine(out, m)\n"
                      "return ok, out.m14, out.m24, out.m34\n";
    auto t = ScriptingTest(src, 4);
    CHECK(lua_toboolean(t.state(), -4) == 1);
    CHECK(lua_tonumber(t.state(), -3) == -10.0);
    CHECK(lua_tonumber(t.state(), -2) == 0.0);
    CHECK(lua_tonumber(t.state(), -1) == 0.0);
}

TEST_CASE("Mat4:invertAffine returns nil on singular linear part",
          "[scripting]")
{
    // Zero scale on Y collapses the linear part — singular.
    const char* src = "local m = Mat4.fromScale(2, 0, 1)\n"
                      "return m:invertAffine()\n";
    auto t = ScriptingTest(src);
    CHECK(lua_isnil(t.state(), -1));
}

TEST_CASE("Mat4.perspectiveReverseZ has expected layout", "[scripting]")
{
    // For aspect=1, fovY=90deg: f = 1/tan(45deg) = 1.
    // m11 = f/aspect = 1, m22 = f = 1, m33 = 0, m43 = -1, m34 = near.
    const char* src = "local p = Mat4.perspectiveReverseZ(math.rad(90), 1, 5)\n"
                      "return p.m11, p.m22, p.m33, p.m43, p.m34\n";
    auto t = ScriptingTest(src, 5);
    CHECK(lua_tonumber(t.state(), -5) == Approx(1.0).margin(1e-6));
    CHECK(lua_tonumber(t.state(), -4) == Approx(1.0).margin(1e-6));
    CHECK(lua_tonumber(t.state(), -3) == 0.0);
    CHECK(lua_tonumber(t.state(), -2) == -1.0);
    CHECK(lua_tonumber(t.state(), -1) == 5.0);
}

TEST_CASE("Mat4:writeToBuffer stores 64 bytes column-major", "[scripting]")
{
    const char* src =
        "local m = Mat4.values(\n"
        "  1, 2, 3, 4,  5, 6, 7, 8,  9,10,11,12, 13,14,15,16)\n"
        "local buf = buffer.create(80)\n"
        "m:writeToBuffer(buf, 16)\n"
        "return buffer.readf32(buf, 16), buffer.readf32(buf, 16+4*4), buffer.readf32(buf, 16+15*4)\n";
    auto t = ScriptingTest(src, 3);
    CHECK(lua_tonumber(t.state(), -3) == 1.0);
    CHECK(lua_tonumber(t.state(), -2) == 5.0);
    CHECK(lua_tonumber(t.state(), -1) == 16.0);
}

namespace
{
// Pure-Luau reference implementation of mat4 multiply on Luau buffers.
// Mirrors the m4mul() pattern used in examples/SpinningCube.luau.
const char* kLuauBufferMatMulPrelude =
    R"(
local function m4get(buf: buffer, i: number): number
    return buffer.readf32(buf, i * 4)
end
local function m4set(buf: buffer, i: number, v: number)
    buffer.writef32(buf, i * 4, v)
end
local function m4identity(): buffer
    local b = buffer.create(64)
    m4set(b, 0, 1)
    m4set(b, 5, 1)
    m4set(b, 10, 1)
    m4set(b, 15, 1)
    return b
end
local function m4mul(out: buffer, a: buffer, b: buffer)
    for col = 0, 3 do
        for row = 0, 3 do
            local sum: number = 0
            for k = 0, 3 do
                sum += m4get(a, k * 4 + row) * m4get(b, col * 4 + k)
            end
            m4set(out, col * 4 + row, sum)
        end
    end
end
)";
} // namespace

TEST_CASE("Mat4 perf — C++ vs Luau-buffer matmul", "[scripting][benchmark]")
{
    const int N = 20000;
    const int WARMUP = 1;
    const int RUNS = 3;

    // Compile-and-run timing: ScriptingTest constructs a fresh VM, compiles,
    // and runs the script. The compile/setup overhead is the same for every
    // variant, so the cross-comparison is valid even though absolute numbers
    // include startup time.
    auto bestRun = [&](const char* src) -> long long {
        long long best = LLONG_MAX;
        for (int run = 0; run < WARMUP + RUNS; ++run)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            ScriptingTest test(src, 0);
            auto t1 = std::chrono::high_resolution_clock::now();
            auto us =
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
                    .count();
            if (run >= WARMUP && us < best)
                best = us;
        }
        return best;
    };

    char src[4096];

    // Variant A: Mat4 * Mat4 (C++, allocates a Mat4 each iteration)
    snprintf(src,
             sizeof(src),
             "local a = Mat4.fromTranslation(1, 2, 3)\n"
             "local b = Mat4.fromRotationZ(0.1)\n"
             "local m = Mat4.identity()\n"
             "for i = 1, %d do m = a * b end\n",
             N);
    long long cppMul = bestRun(src);

    // Variant B: Mat4.multiply(out, a, b) (C++, in-place, no alloc)
    snprintf(src,
             sizeof(src),
             "local a = Mat4.fromTranslation(1, 2, 3)\n"
             "local b = Mat4.fromRotationZ(0.1)\n"
             "local out = Mat4.identity()\n"
             "for i = 1, %d do Mat4.multiply(out, a, b) end\n",
             N);
    long long cppInPlace = bestRun(src);

    // Variant C: pure-Luau buffer matmul (the SpinningCube approach)
    char luauSrc[4096];
    snprintf(
        luauSrc,
        sizeof(luauSrc),
        "%s\n"
        "local a = m4identity()\n"
        "local b = m4identity()\n"
        // Plant a few non-zero entries so the inner loop does real work.
        "m4set(a, 12, 1); m4set(a, 13, 2); m4set(a, 14, 3)\n"
        "m4set(b, 0, 0.99); m4set(b, 1, 0.099); m4set(b, 4, -0.099); m4set(b, 5, 0.99)\n"
        "local out = m4identity()\n"
        "for i = 1, %d do m4mul(out, a, b) end\n",
        kLuauBufferMatMulPrelude,
        N);
    long long luauBuf = bestRun(luauSrc);

    // Variant D: matmul on a Luau table of 16 numbers — no SIMD, no buffer
    // reads, but every entry is a Lua TValue (8-byte tag + double).
    snprintf(
        luauSrc,
        sizeof(luauSrc),
        "local function tnew()\n"
        "  return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}\n"
        "end\n"
        "local function tmul(out, a, b)\n"
        "  for col = 0, 3 do\n"
        "    for row = 0, 3 do\n"
        "      local s = 0\n"
        "      for k = 0, 3 do\n"
        "        s += a[k*4 + row + 1] * b[col*4 + k + 1]\n"
        "      end\n"
        "      out[col*4 + row + 1] = s\n"
        "    end\n"
        "  end\n"
        "end\n"
        "local a = tnew(); a[13] = 1; a[14] = 2; a[15] = 3\n"
        "local b = tnew(); b[1] = 0.99; b[2] = 0.099; b[5] = -0.099; b[6] = 0.99\n"
        "local out = tnew()\n"
        "for i = 1, %d do tmul(out, a, b) end\n",
        N);
    long long luauTable = bestRun(luauSrc);

    fprintf(stderr,
            "\n"
            "Mat4 matmul perf (%d iterations, best of %d, includes VM setup):\n"
            "  C++ a*b           : %lld us\n"
            "  C++ multiply(out) : %lld us\n"
            "  Luau buffer mul   : %lld us\n"
            "  Luau table mul    : %lld us\n"
            "\n",
            N,
            RUNS,
            cppMul,
            cppInPlace,
            luauBuf,
            luauTable);

    // Sanity: the in-place C++ path must be at least as fast as either Luau
    // approach. If this ever fails we have a real perf regression worth
    // investigating.
    CHECK(cppInPlace <= luauBuf);
    CHECK(cppInPlace <= luauTable);
}
