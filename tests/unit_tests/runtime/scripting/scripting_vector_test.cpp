
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include <chrono>
#include <climits>

using namespace rive;

TEST_CASE("vector can be constructed", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("local _vec = Vector.xy(1,2)\n"
                                     "return _vec.x")
                           .state(),
                       -1) == 1.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vector.xy(1,2)\n"
                                     "return _vec.y")
                           .state(),
                       -1) == 2.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vector.xy(33,33)\n"
                                     "return _vec.x")
                           .state(),
                       -1) == 33.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vector.xy(33,33)\n"
                                     "return _vec.y")
                           .state(),
                       -1) == 33.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vector.origin()\n"
                                     "return _vec.x")
                           .state(),
                       -1) == 0.0f);

    CHECK(lua_tonumber(ScriptingTest("local _vec = Vector.origin()\n"
                                     "return _vec.y")
                           .state(),
                       -1) == 0.0f);
}

TEST_CASE("vector static methods work", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest(
                  "return Vector.distance(Vector.origin(),Vector.xy(10,0))")
                  .state(),
              -1) == 10.0f);

    CHECK(
        lua_tonumber(
            ScriptingTest(
                "return Vector.distanceSquared(Vector.origin(),Vector.xy(10,0))")
                .state(),
            -1) == 100.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.dot(Vector.xy(1,0),Vector.xy(-1,0))")
                  .state(),
              -1) == -1.0f);

    CHECK(lua_tonumber(
              ScriptingTest(
                  "return Vector.lerp(Vector.origin(),Vector.xy(1,2), 0.5).x")
                  .state(),
              -1) == 0.5f);

    CHECK(lua_tonumber(
              ScriptingTest(
                  "return Vector.lerp(Vector.origin(),Vector.xy(1,2), 0.5).y")
                  .state(),
              -1) == 1.0f);
}

TEST_CASE("vector static cross/scaleAndAdd/scaleAndSub work", "[scripting]")
{
    // cross: (1,0) x (0,1) = 1
    CHECK(lua_tonumber(ScriptingTest(
                           "return Vector.cross(Vector.xy(1,0),Vector.xy(0,1))")
                           .state(),
                       -1) == 1.0f);

    // cross: (0,1) x (1,0) = -1
    CHECK(lua_tonumber(ScriptingTest(
                           "return Vector.cross(Vector.xy(0,1),Vector.xy(1,0))")
                           .state(),
                       -1) == -1.0f);

    // scaleAndAdd: (1,2) + (3,4)*2 = (7,10)
    CHECK(
        lua_tonumber(
            ScriptingTest(
                "return Vector.scaleAndAdd(Vector.xy(1,2),Vector.xy(3,4),2).x")
                .state(),
            -1) == 7.0f);

    CHECK(
        lua_tonumber(
            ScriptingTest(
                "return Vector.scaleAndAdd(Vector.xy(1,2),Vector.xy(3,4),2).y")
                .state(),
            -1) == 10.0f);

    // scaleAndSub: (7,10) - (3,4)*2 = (1,2)
    CHECK(
        lua_tonumber(
            ScriptingTest(
                "return Vector.scaleAndSub(Vector.xy(7,10),Vector.xy(3,4),2).x")
                .state(),
            -1) == 1.0f);

    CHECK(
        lua_tonumber(
            ScriptingTest(
                "return Vector.scaleAndSub(Vector.xy(7,10),Vector.xy(3,4),2).y")
                .state(),
            -1) == 2.0f);
}

TEST_CASE("vector static length/lengthSquared/normalized work", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("return Vector.length(Vector.xy(3,4))").state(),
              -1) == 5.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.lengthSquared(Vector.xy(3,4))")
                  .state(),
              -1) == 25.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.normalized(Vector.xy(10,0)).x")
                  .state(),
              -1) == 1.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.normalized(Vector.xy(10,0)).y")
                  .state(),
              -1) == 0.0f);
}

TEST_CASE("vector 3D constructor and cross3 work", "[scripting]")
{
    auto t = ScriptingTest("local v = Vector.xyz(1, 2, 3)\n"
                           "return v.x, v.y, v.z\n",
                           3);
    CHECK(lua_tonumber(t.state(), -3) == 1.0f);
    CHECK(lua_tonumber(t.state(), -2) == 2.0f);
    CHECK(lua_tonumber(t.state(), -1) == 3.0f);

    // x cross y = z
    auto c = ScriptingTest(
        "local v = Vector.cross3(Vector.xyz(1,0,0), Vector.xyz(0,1,0))\n"
        "return v.x, v.y, v.z\n",
        3);
    CHECK(lua_tonumber(c.state(), -3) == 0.0f);
    CHECK(lua_tonumber(c.state(), -2) == 0.0f);
    CHECK(lua_tonumber(c.state(), -1) == 1.0f);

    // y cross x = -z
    CHECK(lua_tonumber(ScriptingTest("return Vector.cross3(Vector.xyz(0,1,0), "
                                     "Vector.xyz(1,0,0)).z")
                           .state(),
                       -1) == -1.0f);
}

TEST_CASE("vector magnitude ops use all 3 components", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("return Vector.length(Vector.xyz(1,2,2))").state(),
              -1) == 3.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.lengthSquared(Vector.xyz(1,2,3))")
                  .state(),
              -1) == 14.0f);

    // (1,2,3) . (4,5,6) = 4 + 10 + 18
    CHECK(lua_tonumber(ScriptingTest("return Vector.dot(Vector.xyz(1,2,3), "
                                     "Vector.xyz(4,5,6))")
                           .state(),
                       -1) == 32.0f);

    CHECK(lua_tonumber(ScriptingTest("return Vector.distance("
                                     "Vector.xyz(1,1,1), Vector.xyz(1,1,4))")
                           .state(),
                       -1) == 3.0f);

    CHECK(lua_tonumber(ScriptingTest("return Vector.distanceSquared("
                                     "Vector.xyz(0,0,0), Vector.xyz(1,2,3))")
                           .state(),
                       -1) == 14.0f);

    auto n = ScriptingTest("local v = Vector.normalized(Vector.xyz(0,3,4))\n"
                           "return v.y, v.z\n",
                           2);
    CHECK(lua_tonumber(n.state(), -2) == Approx(0.6f));
    CHECK(lua_tonumber(n.state(), -1) == Approx(0.8f));

    // Zero-length normalize stays the zero vector
    CHECK(lua_tonumber(
              ScriptingTest("return Vector.normalized(Vector.origin()).x")
                  .state(),
              -1) == 0.0f);

    CHECK(lua_tonumber(ScriptingTest("return Vector.lerp(Vector.xyz(0,0,10), "
                                     "Vector.xyz(0,0,20), 0.5).z")
                           .state(),
                       -1) == 15.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.scaleAndAdd(Vector.xyz(0,0,1), "
                            "Vector.xyz(0,0,2), 3).z")
                  .state(),
              -1) == 7.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Vector.scaleAndSub(Vector.xyz(0,0,7), "
                            "Vector.xyz(0,0,2), 3).z")
                  .state(),
              -1) == 1.0f);
}

TEST_CASE("vector writeToBuffer and writeVec4 work", "[scripting]")
{
    auto t = ScriptingTest("local buf = buffer.create(16)\n"
                           "Vector.xyz(1, 2, 3):writeToBuffer(buf, 4)\n"
                           "return buffer.readf32(buf, 4),\n"
                           "    buffer.readf32(buf, 8),\n"
                           "    buffer.readf32(buf, 12)\n",
                           3);
    CHECK(lua_tonumber(t.state(), -3) == 1.0f);
    CHECK(lua_tonumber(t.state(), -2) == 2.0f);
    CHECK(lua_tonumber(t.state(), -1) == 3.0f);

    auto v4 = ScriptingTest("local buf = buffer.create(16)\n"
                            "Vector.xyz(1, 2, 3):writeVec4(buf, 0, 4)\n"
                            "return buffer.readf32(buf, 0),\n"
                            "    buffer.readf32(buf, 12)\n",
                            2);
    CHECK(lua_tonumber(v4.state(), -2) == 1.0f);
    CHECK(lua_tonumber(v4.state(), -1) == 4.0f);

    // Out-of-range offsets error rather than corrupt memory
    CHECK(!lua_toboolean(
        ScriptingTest("local buf = buffer.create(12)\n"
                      "return (pcall(function()\n"
                      "    Vector.origin():writeToBuffer(buf, 4)\n"
                      "end))\n")
            .state(),
        -1));

    CHECK(!lua_toboolean(
        ScriptingTest("local buf = buffer.create(16)\n"
                      "return (pcall(function()\n"
                      "    Vector.origin():writeVec4(buf, 4, 1)\n"
                      "end))\n")
            .state(),
        -1));
}

TEST_CASE("vector fastcall and C binding paths agree", "[scripting]")
{
    // Static Vector.op(...) calls compile to Luau-fork fastcalls; v:op(...)
    // and indirect calls hit the C bindings in lua_vec2d.cpp. Pins the two
    // implementations bit-identical so neither can silently diverge.
    const char* src =
        "local a = Vector.xyz(1.5, -2.25, 3.75)\n"
        "local b = Vector.xyz(-4.5, 5.25, -6.5)\n"
        "local function check(name, x, y)\n"
        "    if x ~= y then\n"
        "        error(`{name} diverged: {x} vs {y}`)\n"
        "    end\n"
        "end\n"
        "-- namecall (C binding) vs static (fastcall)\n"
        "check('length', a:length(), Vector.length(a))\n"
        "check('lengthSquared', a:lengthSquared(), Vector.lengthSquared(a))\n"
        "check('normalized', a:normalized(), Vector.normalized(a))\n"
        "check('distance', a:distance(b), Vector.distance(a, b))\n"
        "check('distanceSquared', a:distanceSquared(b),\n"
        "    Vector.distanceSquared(a, b))\n"
        "check('dot', a:dot(b), Vector.dot(a, b))\n"
        "for _, t in {0, 0.375, 1} do\n"
        "    check(`lerp t={t}`, a:lerp(b, t), Vector.lerp(a, b, t))\n"
        "end\n"
        "-- ops with no instance form: indirect call (C binding) vs direct\n"
        "-- static (fastcall); a table load can't compile to FASTCALL\n"
        "local ind = {\n"
        "    Vector.scaleAndAdd,\n"
        "    Vector.scaleAndSub,\n"
        "    Vector.cross,\n"
        "    Vector.normalized,\n"
        "}\n"
        "check('scaleAndAdd', ind[1](a, b, 2.5), Vector.scaleAndAdd(a, b, "
        "2.5))\n"
        "check('scaleAndSub', ind[2](a, b, 2.5), Vector.scaleAndSub(a, b, "
        "2.5))\n"
        "check('cross', ind[3](a, b), Vector.cross(a, b))\n"
        "-- zero-length normalize stays zero (not NaN) on both paths\n"
        "local z = Vector.origin()\n"
        "check('normalizedZero', ind[4](z), Vector.normalized(z))\n"
        "check('normalizedZeroValue', Vector.normalized(z), z)\n"
        "return true\n";
    CHECK(lua_toboolean(ScriptingTest(src).state(), -1));
}

TEST_CASE("vector indexing work", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return Vector.xy(19, 27)[1]").state(),
                       -1) == 19.0f);

    CHECK(lua_tonumber(ScriptingTest("return Vector.xy(19, 27)[2]").state(),
                       -1) == 27.0f);
}

TEST_CASE("vector methods work", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("return Vector.origin():distance(Vector.xy(10,0))")
                  .state(),
              -1) == 10.0f);

    CHECK(lua_tonumber(
              ScriptingTest(
                  "return Vector.origin():distanceSquared(Vector.xy(10,0))")
                  .state(),
              -1) == 100.0f);

    CHECK(
        lua_tonumber(
            ScriptingTest("return Vector.xy(1,0):dot(Vector.xy(-1,0))").state(),
            -1) == -1.0f);

    CHECK(lua_tonumber(ScriptingTest(
                           "return Vector.origin():lerp(Vector.xy(1,2), 0.5).x")
                           .state(),
                       -1) == 0.5f);

    CHECK(lua_tonumber(ScriptingTest(
                           "return Vector.origin():lerp(Vector.xy(1,2), 0.5).y")
                           .state(),
                       -1) == 1.0f);
}

TEST_CASE("vector meta methods work", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return -Vector.xy(12,13).x").state(),
                       -1) == -12.0f);
    CHECK(lua_tonumber(ScriptingTest("return -Vector.xy(12,13).y").state(),
                       -1) == -13.0f);
    CHECK(
        lua_tonumber(
            ScriptingTest("return (Vector.xy(12,13)+Vector.xy(2,3)).y").state(),
            -1) == 16.0f);
    CHECK(
        lua_tonumber(
            ScriptingTest("return (Vector.xy(12,13)+Vector.xy(2,3)).x").state(),
            -1) == 14.0f);
    CHECK(
        lua_tonumber(
            ScriptingTest("return (Vector.xy(12,13)-Vector.xy(2,3)).y").state(),
            -1) == 10.0f);
    CHECK(
        lua_tonumber(
            ScriptingTest("return (Vector.xy(12,13)-Vector.xy(2,3)).x").state(),
            -1) == 10.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vector.xy(12,13)*3).x").state(),
                       -1) == 36.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vector.xy(12,13)*3).y").state(),
                       -1) == 39.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vector.xy(12,13)/3).x").state(),
                       -1) == 4.0f);
    CHECK(lua_tonumber(ScriptingTest("return (Vector.xy(12,13)/3).y").state(),
                       -1) == Approx(4.3333333f));
    CHECK(lua_toboolean(
              ScriptingTest("return Vector.xy(1,2) == Vector.xy(2,1)").state(),
              -1) == 0);
    CHECK(lua_toboolean(
              ScriptingTest("return Vector.xy(1,2) == Vector.xy(1,2)").state(),
              -1) == 1);
    CHECK(lua_toboolean(
              ScriptingTest("return Vector.xy(1,2) ~= Vector.xy(2,1)").state(),
              -1) == 1);
    CHECK(lua_toboolean(
              ScriptingTest("return Vector.xy(1,2) ~= Vector.xy(1,2)").state(),
              -1) == 0);
}

static int lua_callback(lua_State* L)
{

    unsigned index = lua_tounsignedx(L, lua_upvalueindex(1), nullptr);
    fprintf(stderr, "index from callback upvalue is: %i\n", index);
    return 0;
}

static void* l_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize; /* not used */
    if (nsize == 0)
    {
        free(ptr);
        return NULL;
    }
    else
        return realloc(ptr, nsize);
}

TEST_CASE("closure test", "[scripting]")
{
    lua_State* L;
    const char* source = "callMyFunc()";
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(source, strlen(source), NULL, &bytecodeSize);
    REQUIRE(bytecode != nullptr);
    L = lua_newstate(l_alloc, nullptr);
    // lua_setthreaddata(L, nullptr);
    REQUIRE(L != nullptr);
    luaopen_rive(L);

    lua_pushinteger(L, 222);
    lua_pushcclosurek(L, lua_callback, "callMyFunc", 1, nullptr);
    lua_setglobal(L, "callMyFunc");

    luaL_sandbox(L);
    luaL_sandboxthread(L);

    CHECK(luau_load(L, "test_source", bytecode, bytecodeSize, 0) == LUA_OK);
    free(bytecode);

    auto result = lua_pcall(L, 0, 0, 0);
    CHECK(result == LUA_OK);
    if (result != LUA_OK)
    {
        auto error = lua_tostring(L, -1);
        fprintf(stderr, "  %s\n", error);
    }

    lua_close(L);
}

TEST_CASE("vector fast function benchmark", "[scripting][benchmark]")
{
    const int N = 1000000;
    const int WARMUP = 3;
    const int RUNS = 5;
    char script[2048];

    struct BenchResult
    {
        const char* name;
        long long staticUs;
        long long instanceUs;
    };

    auto benchOp = [&](const char* name,
                       const char* staticBody,
                       const char* instanceBody) -> BenchResult {
        // Compile both scripts once (don't time compilation)
        snprintf(script, sizeof(script), "%s", staticBody);
        snprintf(script, sizeof(script), "%s", instanceBody);

        // Warm up + measure static calls
        long long bestStatic = LLONG_MAX;
        for (int run = 0; run < WARMUP + RUNS; run++)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            ScriptingTest test(staticBody);
            auto t1 = std::chrono::high_resolution_clock::now();
            auto us =
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
                    .count();
            if (run >= WARMUP && us < bestStatic)
                bestStatic = us;
        }

        // Warm up + measure instance calls
        long long bestInstance = LLONG_MAX;
        for (int run = 0; run < WARMUP + RUNS; run++)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            ScriptingTest test(instanceBody);
            auto t1 = std::chrono::high_resolution_clock::now();
            auto us =
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
                    .count();
            if (run >= WARMUP && us < bestInstance)
                bestInstance = us;
        }

        return {name, bestStatic, bestInstance};
    };

    char dotStaticSrc[1024], dotInstanceSrc[1024];
    snprintf(dotStaticSrc,
             sizeof(dotStaticSrc),
             "local a = Vector.xy(3, 4)\n"
             "local b = Vector.xy(1, -2)\n"
             "local r = 0\n"
             "for i = 1, %d do r = r + Vector.dot(a, b) end\n"
             "return r\n",
             N);
    snprintf(dotInstanceSrc,
             sizeof(dotInstanceSrc),
             "local a = Vector.xy(3, 4)\n"
             "local b = Vector.xy(1, -2)\n"
             "local r = 0\n"
             "for i = 1, %d do r = r + a:dot(b) end\n"
             "return r\n",
             N);

    char distStaticSrc[1024], distInstanceSrc[1024];
    snprintf(distStaticSrc,
             sizeof(distStaticSrc),
             "local a = Vector.xy(3, 4)\n"
             "local b = Vector.xy(1, -2)\n"
             "local r = 0\n"
             "for i = 1, %d do r = r + Vector.distance(a, b) end\n"
             "return r\n",
             N);
    snprintf(distInstanceSrc,
             sizeof(distInstanceSrc),
             "local a = Vector.xy(3, 4)\n"
             "local b = Vector.xy(1, -2)\n"
             "local r = 0\n"
             "for i = 1, %d do r = r + a:distance(b) end\n"
             "return r\n",
             N);

    char lenStaticSrc[1024], lenInstanceSrc[1024];
    snprintf(lenStaticSrc,
             sizeof(lenStaticSrc),
             "local a = Vector.xy(3, 4)\n"
             "local r = 0\n"
             "for i = 1, %d do r = r + Vector.length(a) end\n"
             "return r\n",
             N);
    snprintf(lenInstanceSrc,
             sizeof(lenInstanceSrc),
             "local a = Vector.xy(3, 4)\n"
             "local r = 0\n"
             "for i = 1, %d do r = r + a:length() end\n"
             "return r\n",
             N);

    char lerpStaticSrc[1024], lerpInstanceSrc[1024];
    snprintf(lerpStaticSrc,
             sizeof(lerpStaticSrc),
             "local a = Vector.xy(0, 0)\n"
             "local b = Vector.xy(10, 20)\n"
             "local r\n"
             "for i = 1, %d do r = Vector.lerp(a, b, 0.5) end\n"
             "return r.x\n",
             N);
    snprintf(lerpInstanceSrc,
             sizeof(lerpInstanceSrc),
             "local a = Vector.xy(0, 0)\n"
             "local b = Vector.xy(10, 20)\n"
             "local r\n"
             "for i = 1, %d do r = a:lerp(b, 0.5) end\n"
             "return r.x\n",
             N);

    auto dotR = benchOp("dot", dotStaticSrc, dotInstanceSrc);
    auto distR = benchOp("distance", distStaticSrc, distInstanceSrc);
    auto lenR = benchOp("length", lenStaticSrc, lenInstanceSrc);
    auto lerpR = benchOp("lerp", lerpStaticSrc, lerpInstanceSrc);

    BenchResult results[] = {dotR, distR, lenR, lerpR};

    fprintf(stderr,
            "\n=== Vector Fast Function Benchmark (%d iters, best of %d "
            "runs) ===\n",
            N,
            RUNS);
    fprintf(stderr,
            "  %-20s %12s %12s %10s\n",
            "Operation",
            "FASTCALL",
            "namecall",
            "Speedup");
    fprintf(stderr, "  %-20s %12s %12s %10s\n", "", "(us)", "(us)", "");
    for (auto& r : results)
    {
        fprintf(stderr,
                "  %-20s %12lld %12lld %9.2fx\n",
                r.name,
                r.staticUs,
                r.instanceUs,
                (double)r.instanceUs / r.staticUs);
    }
    fprintf(stderr, "  =================================================\n\n");

    // Sanity: static dot result is correct
    ScriptingTest dotCheck(dotStaticSrc);
    CHECK(lua_tonumber(dotCheck.state(), -1) == Approx(-5.0f * N));
}
