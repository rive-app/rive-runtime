
#include "catch.hpp"
#include "lua.h"
#include "scripting_test_utilities.hpp"

using namespace rive;

TEST_CASE("paint can be constructed", "[scripting]")
{
    CHECK(lua_userdatatag(
              ScriptingTest(
                  // clang-format off
                    "return Paint.with({\n"
                        "style = 'stroke',\n"
                        "join = 'round',\n"
                        "cap = 'butt',\n"
                        "blendMode = 'srcOver',\n"
                        "color = 0xffff0000,\n"
                        "thickness = 3,\n"
                        "feather = 0,\n"
                        "gradient = Gradient.radial(Vector.origin(), 20.0, {\n"
                            "{ position = 0.0, color = Color.rgba(255, 0, 0, 255) },\n"
                            "{ position = 1.0, color = Color.rgba(255, 0, 0, 0) },\n"
                        "}),\n"
                    "})\n"
                  // clang-format on
                  )
                  .state(),
              -1) == ScriptedPaint::luaTag);

    CHECK(lua_userdatatag(ScriptingTest(
                              // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.cap = 'round'\n"
                    "return paint\n"
                              // clang-format on
                              )
                              .state(),
                          -1) == ScriptedPaint::luaTag);

    CHECK(strcmp(lua_tostring(ScriptingTest(
                                  // clang-format off
                    "local paint = Paint.new()\n"
                    "return paint.style\n"
                                  // clang-format on
                                  )
                                  .state(),
                              -1),
                 "fill") == 0);

    CHECK(strcmp(lua_tostring(ScriptingTest(
                                  // clang-format off
                    "local paint = Paint.with({style='stroke'})\n"
                    "return paint.style\n"
                                  // clang-format on
                                  )
                                  .state(),
                              -1),
                 "stroke") == 0);

    CHECK(strcmp(lua_tostring(ScriptingTest(
                                  // clang-format off
                    "local paint = Paint.with({style='stroke'})\n"
                    "return paint.join\n"
                                  // clang-format on
                                  )
                                  .state(),
                              -1),
                 "miter") == 0);

    CHECK(strcmp(lua_tostring(ScriptingTest(
                                  // clang-format off
                    "local paint = Paint.with({join='round'})\n"
                    "return paint.join\n"
                                  // clang-format on
                                  )
                                  .state(),
                              -1),
                 "round") == 0);

    CHECK(strcmp(lua_tostring(ScriptingTest(
                                  // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.join = 'bevel'\n"
                    "return paint.join\n"
                                  // clang-format on
                                  )
                                  .state(),
                              -1),
                 "bevel") == 0);

    CHECK(lua_tonumber(ScriptingTest(
                           // clang-format off
                    "return Paint.new().thickness\n"

                           // clang-format on
                           )
                           .state(),
                       -1) == 1);

    CHECK(lua_tonumber(ScriptingTest(
                           // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.thickness = 22\n"
                    "return paint.thickness\n"
                           // clang-format on
                           )
                           .state(),
                       -1) == 22);

    CHECK(lua_tounsigned(ScriptingTest(
                             // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.color = Color.rgb(255, 128, 64)\n"
                    "return paint.color\n"
                             // clang-format on
                             )
                             .state(),
                         -1) == 0xffff8040);

    CHECK(lua_tonumber(ScriptingTest(
                           // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.feather = 0.222\n"
                    "return paint.feather\n"
                           // clang-format on
                           )
                           .state(),
                       -1) == Approx(0.222));

    CHECK(lua_isnil(ScriptingTest(
                        // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.feather = 0.222\n"
                    "return paint.gradient\n"
                        // clang-format on
                        )
                        .state(),
                    -1));

    CHECK(lua_userdatatag(
              ScriptingTest(
                  // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.gradient = Gradient.radial(Vector.origin(), 20.0, {\n"
                        "{ position = 0.0, color = Color.rgba(255, 0, 0, 255) },\n"
                        "{ position = 1.0, color = Color.rgba(255, 0, 0, 0) },\n"
                    "})\n"
                    "return paint.gradient\n"
                  // clang-format on
                  )
                  .state(),
              -1) == ScriptedGradient::luaTag);

    CHECK(lua_userdatatag(
              ScriptingTest(
                  // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.gradient = Gradient.radial(Vector.origin(), 20.0, {\n"
                        "{ position = 0.0, color = Color.rgba(255, 0, 0, 255) },\n"
                        "{ position = 1.0, color = Color.rgba(255, 0, 0, 0) },\n"
                    "})\n"
                    "return paint:copy().gradient\n"
                  // clang-format on
                  )
                  .state(),
              -1) == ScriptedGradient::luaTag);
}

TEST_CASE("paint gradients can be cleared", "[scripting]")
{
    CHECK(lua_type(
              ScriptingTest(
                  // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.gradient = Gradient.radial(Vector.origin(), 20.0, {\n"
                        "{ position = 0.0, color = Color.rgba(255, 0, 0, 255) },\n"
                        "{ position = 1.0, color = Color.rgba(255, 0, 0, 0) },\n"
                    "})\n"
                    "return paint:copy({gradient=false}).gradient\n"
                  // clang-format on
                  )
                  .state(),
              -1) == LUA_TNIL);

    CHECK(lua_type(
              ScriptingTest(
                  // clang-format off
                    "local paint = Paint.new()\n"
                    "paint.gradient = Gradient.radial(Vector.origin(), 20.0, {\n"
                        "{ position = 0.0, color = Color.rgba(255, 0, 0, 255) },\n"
                        "{ position = 1.0, color = Color.rgba(255, 0, 0, 0) },\n"
                    "})\n"
                    "local paintCopy = paint:copy()\n"
                    "paintCopy.gradient = nil\n"
                    "return paintCopy.gradient\n"
                  // clang-format on
                  )
                  .state(),
              -1) == LUA_TNIL);
}

TEST_CASE("paint gradient transform round trips", "[scripting]")
{
    ScriptingTest vm(
        // clang-format off
        "local paint = Paint.with({\n"
            "gradientTransform = Mat2D.values(0.8, 0.3, -0.3, 0.8, 20, -10),\n"
        "})\n"
        "local m = paint.gradientTransform\n"
        "return m.xx, m.xy, m.yx, m.yy, m.tx, m.ty\n"
        // clang-format on
        ,
        6);
    lua_State* L = vm.state();
    CHECK((float)lua_tonumber(L, -6) == Approx(0.8f));
    CHECK((float)lua_tonumber(L, -5) == Approx(0.3f));
    CHECK((float)lua_tonumber(L, -4) == Approx(-0.3f));
    CHECK((float)lua_tonumber(L, -3) == Approx(0.8f));
    CHECK((float)lua_tonumber(L, -2) == Approx(20.0f));
    CHECK((float)lua_tonumber(L, -1) == Approx(-10.0f));
}

TEST_CASE("paint gradient transform survives assignment and copy",
          "[scripting]")
{
    ScriptingTest vm(
        // clang-format off
        "local paint = Paint.new()\n"
        "paint.gradientTransform = Mat2D.withTranslation(12, 34)\n"
        "local m = paint:copy().gradientTransform\n"
        "return m.tx, m.ty\n"
        // clang-format on
        ,
        2);
    lua_State* L = vm.state();
    CHECK((float)lua_tonumber(L, -2) == Approx(12.0f));
    CHECK((float)lua_tonumber(L, -1) == Approx(34.0f));
}
