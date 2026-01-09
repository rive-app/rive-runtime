
#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

TEST_CASE("mesh can be constructed", "[scripting]")
{
    CHECK(
        lua_userdatatag(
            ScriptingTest(
                // clang-format off
                            "local _vertices = VertexBuffer()\n"
                            "_vertices:add(Vector.xy(0,0),Vector.xy(1,0),Vector.xy(1,1))\n"
                            "local _uvs = VertexBuffer()\n"
                            "_uvs:add(Vector.xy(0,0))\n"
                            "_uvs:add(Vector.xy(1,0))\n"
                            "_uvs:add(Vector.xy(1,1))\n"
                            "local indices = TriangleBuffer()\n"
                            "indices:add(0, 1, 2)\n"
                            "return indices"
                // clang-format on
                )
                .state(),
            -1) == ScriptedTriangleBuffer::luaTag);
}

TEST_CASE("ImageSampler can be created", "[scripting]")
{
    CHECK(
        lua_userdatatag(ScriptingTest(
                            // clang-format off
                            "return ImageSampler('clamp', 'mirror', 'nearest')"
                            // clang-format on
                            )
                            .state(),
                        -1) == ScriptedImageSampler::luaTag);
}
