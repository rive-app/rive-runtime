
#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

// --------------------------------------------------------------------------
// buffer.readf16 / buffer.writef16
// --------------------------------------------------------------------------

TEST_CASE("buffer.writef16 and readf16 round-trip", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, 1.5)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == Approx(1.5));
}

TEST_CASE("buffer.readf16 zero", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, 0)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == 0.0);
}

TEST_CASE("buffer.readf16 negative zero", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, -0.0)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    // -0.0 == 0.0 in IEEE, but signbit should be set
    CHECK(result == 0.0);
}

TEST_CASE("buffer.writef16 and readf16 negative value", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, -3.25)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == Approx(-3.25));
}

TEST_CASE("buffer.writef16 and readf16 small denormal", "[scripting]")
{
    // Smallest positive f16 denormal: 2^-24 ≈ 5.96e-8
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, 5.96046448e-8)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == Approx(5.96046448e-8).margin(1e-9));
}

TEST_CASE("buffer.writef16 infinity", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, math.huge)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == Approx(std::numeric_limits<double>::infinity()));
}

TEST_CASE("buffer.writef16 negative infinity", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, -math.huge)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == Approx(-std::numeric_limits<double>::infinity()));
}

TEST_CASE("buffer.writef16 overflow to infinity", "[scripting]")
{
    // f16 max is 65504; values above should overflow to inf
    ScriptingTest vm("local b = buffer.create(2)\n"
                     "buffer.writef16(b, 0, 100000)\n"
                     "return buffer.readf16(b, 0)",
                     1);
    lua_Number result = lua_tonumber(vm.state(), -1);
    CHECK(result == Approx(std::numeric_limits<double>::infinity()));
}

TEST_CASE("buffer.readf16 multiple offsets", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(6)\n"
                     "buffer.writef16(b, 0, 1.0)\n"
                     "buffer.writef16(b, 2, 2.0)\n"
                     "buffer.writef16(b, 4, 3.0)\n"
                     "return buffer.readf16(b, 0), "
                     "buffer.readf16(b, 2), "
                     "buffer.readf16(b, 4)",
                     3);
    CHECK(lua_tonumber(vm.state(), -3) == Approx(1.0));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(2.0));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(3.0));
}

TEST_CASE("buffer.readf16 out of bounds", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(1)\n"
                     "return buffer.readf16(b, 0)",
                     1,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("out of bounds") != std::string::npos);
}

TEST_CASE("buffer.writef16 out of bounds", "[scripting]")
{
    ScriptingTest vm("local b = buffer.create(1)\n"
                     "buffer.writef16(b, 0, 1.0)",
                     0,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("out of bounds") != std::string::npos);
}

// --------------------------------------------------------------------------
// buffer.stridedcopy
// --------------------------------------------------------------------------

TEST_CASE("buffer.stridedcopy basic", "[scripting]")
{
    // Copy 3 floats from stride-8 (float + 4 padding) to packed stride-4
    ScriptingTest vm("local src = buffer.create(24)\n" // 3 elements * 8 stride
                     "buffer.writef32(src, 0,  10.0)\n"
                     "buffer.writef32(src, 8,  20.0)\n"
                     "buffer.writef32(src, 16, 30.0)\n"
                     "local dst = buffer.create(12)\n" // 3 elements * 4 stride
                     "buffer.stridedcopy(dst, 0, 4, src, 0, 8, 4, 3)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4), "
                     "buffer.readf32(dst, 8)",
                     3);
    CHECK(lua_tonumber(vm.state(), -3) == Approx(10.0));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(20.0));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(30.0));
}

TEST_CASE("buffer.stridedcopy interleave", "[scripting]")
{
    // Interleave two packed arrays into stride-8 buffer
    // positions: [1.0, 2.0, 3.0]  -> offsets 0, 8, 16
    // colors:    [4.0, 5.0, 6.0]  -> offsets 4, 12, 20
    ScriptingTest vm("local pos = buffer.create(12)\n"
                     "buffer.writef32(pos, 0, 1.0)\n"
                     "buffer.writef32(pos, 4, 2.0)\n"
                     "buffer.writef32(pos, 8, 3.0)\n"
                     "local col = buffer.create(12)\n"
                     "buffer.writef32(col, 0, 4.0)\n"
                     "buffer.writef32(col, 4, 5.0)\n"
                     "buffer.writef32(col, 8, 6.0)\n"
                     "local interleaved = buffer.create(24)\n"
                     "buffer.stridedcopy(interleaved, 0, 8, pos, 0, 4, 4, 3)\n"
                     "buffer.stridedcopy(interleaved, 4, 8, col, 0, 4, 4, 3)\n"
                     "return buffer.readf32(interleaved, 0),  "
                     "buffer.readf32(interleaved, 4),  "
                     "buffer.readf32(interleaved, 8),  "
                     "buffer.readf32(interleaved, 12), "
                     "buffer.readf32(interleaved, 16), "
                     "buffer.readf32(interleaved, 20)",
                     6);
    CHECK(lua_tonumber(vm.state(), -6) == Approx(1.0)); // pos[0]
    CHECK(lua_tonumber(vm.state(), -5) == Approx(4.0)); // col[0]
    CHECK(lua_tonumber(vm.state(), -4) == Approx(2.0)); // pos[1]
    CHECK(lua_tonumber(vm.state(), -3) == Approx(5.0)); // col[1]
    CHECK(lua_tonumber(vm.state(), -2) == Approx(3.0)); // pos[2]
    CHECK(lua_tonumber(vm.state(), -1) == Approx(6.0)); // col[2]
}

TEST_CASE("buffer.stridedcopy zero count", "[scripting]")
{
    // Should be a no-op
    ScriptingTest vm("local b = buffer.create(4)\n"
                     "buffer.writef32(b, 0, 99.0)\n"
                     "buffer.stridedcopy(b, 0, 4, b, 0, 4, 4, 0)\n"
                     "return buffer.readf32(b, 0)",
                     1);
    CHECK(lua_tonumber(vm.state(), -1) == Approx(99.0));
}

TEST_CASE("buffer.stridedcopy multi-byte element", "[scripting]")
{
    // Copy vec2 (8 bytes) elements with stride 12 (vec2 + 4 padding)
    ScriptingTest vm("local src = buffer.create(24)\n"
                     "buffer.writef32(src, 0,  1.0)\n"
                     "buffer.writef32(src, 4,  2.0)\n"
                     "buffer.writef32(src, 12, 3.0)\n"
                     "buffer.writef32(src, 16, 4.0)\n"
                     "local dst = buffer.create(16)\n"
                     "buffer.stridedcopy(dst, 0, 8, src, 0, 12, 8, 2)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4), "
                     "buffer.readf32(dst, 8), buffer.readf32(dst, 12)",
                     4);
    CHECK(lua_tonumber(vm.state(), -4) == Approx(1.0));
    CHECK(lua_tonumber(vm.state(), -3) == Approx(2.0));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(3.0));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(4.0));
}

TEST_CASE("buffer.stridedcopy out of bounds src", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(4)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.stridedcopy(dst, 0, 4, src, 0, 4, 4, 2)\n",
                     0,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("out of bounds") != std::string::npos);
}

TEST_CASE("buffer.stridedcopy out of bounds dst", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "local dst = buffer.create(4)\n"
                     "buffer.stridedcopy(dst, 0, 4, src, 0, 4, 4, 2)\n",
                     0,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("out of bounds") != std::string::npos);
}

// --------------------------------------------------------------------------
// buffer.convert
// --------------------------------------------------------------------------

TEST_CASE("buffer.convert f32 to f16", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(12)\n"
                     "buffer.writef32(src, 0, 1.0)\n"
                     "buffer.writef32(src, 4, 0.5)\n"
                     "buffer.writef32(src, 8, -2.0)\n"
                     "local dst = buffer.create(6)\n"
                     "buffer.convert(dst, 0, 'f16', src, 0, 'f32', 3)\n"
                     "return buffer.readf16(dst, 0), "
                     "buffer.readf16(dst, 2), "
                     "buffer.readf16(dst, 4)",
                     3);
    CHECK(lua_tonumber(vm.state(), -3) == Approx(1.0));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(0.5));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(-2.0));
}

TEST_CASE("buffer.convert f16 to f32", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(4)\n"
                     "buffer.writef16(src, 0, 3.5)\n"
                     "buffer.writef16(src, 2, -1.25)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.convert(dst, 0, 'f32', src, 0, 'f16', 2)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == Approx(3.5));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(-1.25));
}

TEST_CASE("buffer.convert u8norm to f32", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(3)\n"
                     "buffer.writeu8(src, 0, 0)\n"
                     "buffer.writeu8(src, 1, 128)\n"
                     "buffer.writeu8(src, 2, 255)\n"
                     "local dst = buffer.create(12)\n"
                     "buffer.convert(dst, 0, 'f32', src, 0, 'u8norm', 3)\n"
                     "return buffer.readf32(dst, 0), "
                     "buffer.readf32(dst, 4), "
                     "buffer.readf32(dst, 8)",
                     3);
    CHECK(lua_tonumber(vm.state(), -3) == Approx(0.0));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(128.0 / 255.0).margin(0.001));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(1.0));
}

TEST_CASE("buffer.convert f32 to u8norm", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(12)\n"
                     "buffer.writef32(src, 0, 0.0)\n"
                     "buffer.writef32(src, 4, 0.5)\n"
                     "buffer.writef32(src, 8, 1.0)\n"
                     "local dst = buffer.create(3)\n"
                     "buffer.convert(dst, 0, 'u8norm', src, 0, 'f32', 3)\n"
                     "return buffer.readu8(dst, 0), "
                     "buffer.readu8(dst, 1), "
                     "buffer.readu8(dst, 2)",
                     3);
    CHECK(lua_tonumber(vm.state(), -3) == 0);
    CHECK(lua_tonumber(vm.state(), -2) == 128); // 0.5 * 255 + 0.5 = 128.0
    CHECK(lua_tonumber(vm.state(), -1) == 255);
}

TEST_CASE("buffer.convert u8norm clamps out-of-range f32", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "buffer.writef32(src, 0, -0.5)\n"
                     "buffer.writef32(src, 4, 1.5)\n"
                     "local dst = buffer.create(2)\n"
                     "buffer.convert(dst, 0, 'u8norm', src, 0, 'f32', 2)\n"
                     "return buffer.readu8(dst, 0), buffer.readu8(dst, 1)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == 0);   // clamped to 0
    CHECK(lua_tonumber(vm.state(), -1) == 255); // clamped to 1
}

TEST_CASE("buffer.convert u16norm to f32", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(4)\n"
                     "buffer.writeu16(src, 0, 0)\n"
                     "buffer.writeu16(src, 2, 65535)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.convert(dst, 0, 'f32', src, 0, 'u16norm', 2)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == Approx(0.0));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(1.0));
}

TEST_CASE("buffer.convert same format is memcpy", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "buffer.writef32(src, 0, 42.0)\n"
                     "buffer.writef32(src, 4, 99.0)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.convert(dst, 0, 'f32', src, 0, 'f32', 2)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == Approx(42.0));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(99.0));
}

TEST_CASE("buffer.convert zero count", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(4)\n"
                     "local dst = buffer.create(4)\n"
                     "buffer.writef32(dst, 0, 77.0)\n"
                     "buffer.convert(dst, 0, 'f16', src, 0, 'f32', 0)\n"
                     "return buffer.readf32(dst, 0)",
                     1);
    // dst should be untouched
    CHECK(lua_tonumber(vm.state(), -1) == Approx(77.0));
}

TEST_CASE("buffer.convert u8 to u16", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(2)\n"
                     "buffer.writeu8(src, 0, 200)\n"
                     "buffer.writeu8(src, 1, 0)\n"
                     "local dst = buffer.create(4)\n"
                     "buffer.convert(dst, 0, 'u16', src, 0, 'u8', 2)\n"
                     "return buffer.readu16(dst, 0), buffer.readu16(dst, 2)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == 200);
    CHECK(lua_tonumber(vm.state(), -1) == 0);
}

TEST_CASE("buffer.convert out of bounds src", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(2)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.convert(dst, 0, 'f32', src, 0, 'f32', 1)\n",
                     0,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("out of bounds") != std::string::npos);
}

TEST_CASE("buffer.convert out of bounds dst", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "local dst = buffer.create(2)\n"
                     "buffer.convert(dst, 0, 'f32', src, 0, 'f32', 1)\n",
                     0,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("out of bounds") != std::string::npos);
}

TEST_CASE("buffer.convert unknown format", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(4)\n"
                     "local dst = buffer.create(4)\n"
                     "buffer.convert(dst, 0, 'rgb8', src, 0, 'f32', 1)\n",
                     0,
                     true);
    auto error = lua_tostring(vm.state(), -1);
    CHECK(std::string(error).find("unknown buffer format") !=
          std::string::npos);
}

TEST_CASE("buffer.convert i8norm round-trip", "[scripting]")
{
    // f32 -> i8norm -> f32 should preserve sign and approximate magnitude
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "buffer.writef32(src, 0, -1.0)\n"
                     "buffer.writef32(src, 4, 0.5)\n"
                     "local mid = buffer.create(2)\n"
                     "buffer.convert(mid, 0, 'i8norm', src, 0, 'f32', 2)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.convert(dst, 0, 'f32', mid, 0, 'i8norm', 2)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == Approx(-1.0).margin(0.01));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(0.5).margin(0.01));
}

TEST_CASE("buffer.convert i16norm round-trip", "[scripting]")
{
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "buffer.writef32(src, 0, -0.75)\n"
                     "buffer.writef32(src, 4, 0.25)\n"
                     "local mid = buffer.create(4)\n"
                     "buffer.convert(mid, 0, 'i16norm', src, 0, 'f32', 2)\n"
                     "local dst = buffer.create(8)\n"
                     "buffer.convert(dst, 0, 'f32', mid, 0, 'i16norm', 2)\n"
                     "return buffer.readf32(dst, 0), buffer.readf32(dst, 4)",
                     2);
    CHECK(lua_tonumber(vm.state(), -2) == Approx(-0.75).margin(0.001));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(0.25).margin(0.001));
}

// --------------------------------------------------------------------------
// buffer.convert with components
// --------------------------------------------------------------------------

TEST_CASE("buffer.convert f32 to f16 with components=2", "[scripting]")
{
    // Convert 3 vec2s (6 scalars) from f32 to f16
    ScriptingTest vm("local src = buffer.create(24)\n" // 3 vec2s * 8 bytes
                     "buffer.writef32(src, 0,  1.0)\n"
                     "buffer.writef32(src, 4,  2.0)\n"
                     "buffer.writef32(src, 8,  3.0)\n"
                     "buffer.writef32(src, 12, 4.0)\n"
                     "buffer.writef32(src, 16, 5.0)\n"
                     "buffer.writef32(src, 20, 6.0)\n"
                     "local dst = buffer.create(12)\n" // 3 vec2s * 4 bytes
                     "buffer.convert(dst, 0, 'f16', src, 0, 'f32', 3, 2)\n"
                     "return buffer.readf16(dst, 0), buffer.readf16(dst, 2), "
                     "buffer.readf16(dst, 4), buffer.readf16(dst, 6), "
                     "buffer.readf16(dst, 8), buffer.readf16(dst, 10)",
                     6);
    CHECK(lua_tonumber(vm.state(), -6) == Approx(1.0));
    CHECK(lua_tonumber(vm.state(), -5) == Approx(2.0));
    CHECK(lua_tonumber(vm.state(), -4) == Approx(3.0));
    CHECK(lua_tonumber(vm.state(), -3) == Approx(4.0));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(5.0));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(6.0));
}

// --------------------------------------------------------------------------
// buffer.convert with strides (interleaved vertex data)
// --------------------------------------------------------------------------

TEST_CASE("buffer.convert strided f32 to f16", "[scripting]")
{
    // Interleaved vertex: [pos.x(f32) pos.y(f32) uv.x(f32) uv.y(f32)] = 16 byte
    // stride Convert only UVs (at offset 8) from f32 to f16 in-place style
    ScriptingTest vm(
        "local src = buffer.create(32)\n"   // 2 vertices * 16 stride
        "buffer.writef32(src, 0,  100.0)\n" // pos0.x
        "buffer.writef32(src, 4,  200.0)\n" // pos0.y
        "buffer.writef32(src, 8,  0.5)\n"   // uv0.x
        "buffer.writef32(src, 12, 0.75)\n"  // uv0.y
        "buffer.writef32(src, 16, 300.0)\n" // pos1.x
        "buffer.writef32(src, 20, 400.0)\n" // pos1.y
        "buffer.writef32(src, 24, 0.25)\n"  // uv1.x
        "buffer.writef32(src, 28, 1.0)\n"   // uv1.y
        "local dst = buffer.create(8)\n"    // 2 * 2 f16 = 8 bytes packed
        "buffer.convert(dst, 0, 'f16', src, 8, 'f32', 2, 2, 4, 16)\n"
        "return buffer.readf16(dst, 0), buffer.readf16(dst, 2), "
        "buffer.readf16(dst, 4), buffer.readf16(dst, 6)",
        4);
    CHECK(lua_tonumber(vm.state(), -4) == Approx(0.5));  // uv0.x
    CHECK(lua_tonumber(vm.state(), -3) == Approx(0.75)); // uv0.y
    CHECK(lua_tonumber(vm.state(), -2) == Approx(0.25)); // uv1.x
    CHECK(lua_tonumber(vm.state(), -1) == Approx(1.0));  // uv1.y
}

TEST_CASE("buffer.convert strided u8norm to f32", "[scripting]")
{
    // RGBA8 colors at stride 8 (4 bytes color + 4 bytes padding)
    // Convert to packed f32
    ScriptingTest vm(
        "local src = buffer.create(16)\n" // 2 elements * 8 stride
        "buffer.writeu8(src, 0, 255)\n"   // r0
        "buffer.writeu8(src, 1, 0)\n"     // g0
        "buffer.writeu8(src, 2, 128)\n"   // b0
        "buffer.writeu8(src, 3, 255)\n"   // a0
        "buffer.writeu8(src, 8, 0)\n"     // r1
        "buffer.writeu8(src, 9, 255)\n"   // g1
        "buffer.writeu8(src, 10, 0)\n"    // b1
        "buffer.writeu8(src, 11, 128)\n"  // a1
        "local dst = buffer.create(32)\n" // 2 * 4 f32 = 32 bytes
        "buffer.convert(dst, 0, 'f32', src, 0, 'u8norm', 2, 4, 16, 8)\n"
        "return buffer.readf32(dst, 0), buffer.readf32(dst, 4), "
        "buffer.readf32(dst, 8), buffer.readf32(dst, 12), "
        "buffer.readf32(dst, 16), buffer.readf32(dst, 20), "
        "buffer.readf32(dst, 24), buffer.readf32(dst, 28)",
        8);
    CHECK(lua_tonumber(vm.state(), -8) == Approx(1.0)); // r0
    CHECK(lua_tonumber(vm.state(), -7) == Approx(0.0)); // g0
    CHECK(lua_tonumber(vm.state(), -6) ==
          Approx(128.0 / 255.0).margin(0.01));          // b0
    CHECK(lua_tonumber(vm.state(), -5) == Approx(1.0)); // a0
    CHECK(lua_tonumber(vm.state(), -4) == Approx(0.0)); // r1
    CHECK(lua_tonumber(vm.state(), -3) == Approx(1.0)); // g1
    CHECK(lua_tonumber(vm.state(), -2) == Approx(0.0)); // b1
    CHECK(lua_tonumber(vm.state(), -1) ==
          Approx(128.0 / 255.0).margin(0.01)); // a1
}

TEST_CASE("buffer.convert components=1 is default behavior", "[scripting]")
{
    // Explicit components=1 should match the default (no components arg)
    ScriptingTest vm("local src = buffer.create(8)\n"
                     "buffer.writef32(src, 0, 1.5)\n"
                     "buffer.writef32(src, 4, -2.5)\n"
                     "local dst1 = buffer.create(4)\n"
                     "local dst2 = buffer.create(4)\n"
                     "buffer.convert(dst1, 0, 'f16', src, 0, 'f32', 2)\n"
                     "buffer.convert(dst2, 0, 'f16', src, 0, 'f32', 2, 1)\n"
                     "return buffer.readf16(dst1, 0), buffer.readf16(dst1, 2), "
                     "buffer.readf16(dst2, 0), buffer.readf16(dst2, 2)",
                     4);
    CHECK(lua_tonumber(vm.state(), -4) == Approx(1.5));
    CHECK(lua_tonumber(vm.state(), -3) == Approx(-2.5));
    CHECK(lua_tonumber(vm.state(), -2) == Approx(1.5));
    CHECK(lua_tonumber(vm.state(), -1) == Approx(-2.5));
}
