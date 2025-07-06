
#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

TEST_CASE("mat2d can be constructed", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return Mat2D(1,2,3,4,5,6).tx").state(),
                       -1) == 5.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D(1,2,3,4,5,6).ty").state(),
                       -1) == 6.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D(1,2,3,4,5,6).xx").state(),
                       -1) == 1.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D(1,2,3,4,5,6).xy").state(),
                       -1) == 2.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D(1,2,3,4,5,6).yx").state(),
                       -1) == 3.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D(1,2,3,4,5,6).yy").state(),
                       -1) == 4.0f);
}

TEST_CASE("mat2d static methods work", "[scripting]")
{
    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withTranslation(10, 20).tx").state(),
              -1) == 10.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withTranslation(10, 20).ty").state(),
              -1) == 20.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withTranslation(Vec2D(10, 20)).tx")
                  .state(),
              -1) == 10.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withTranslation(Vec2D(10, 20)).ty")
                  .state(),
              -1) == 20.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withScale(Vec2D(10, 20)).xx").state(),
              -1) == 10.0f);

    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withScale(Vec2D(10, 20)).yy").state(),
              -1) == 20.0f);

    CHECK(
        lua_tonumber(ScriptingTest("return Mat2D.withScale(10, 20).xx").state(),
                     -1) == 10.0f);

    CHECK(
        lua_tonumber(ScriptingTest("return Mat2D.withScale(10, 20).yy").state(),
                     -1) == 20.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D.withScale(3).yy").state(),
                       -1) == 3.0f);

    CHECK(lua_tonumber(ScriptingTest("return Mat2D.withScale(3).xx").state(),
                       -1) == 3.0f);

    // scaleAndTranslation(scale:Vec2D, translation:Vec2D)
    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withScaleAndTranslation(Vec2D(2, "
                            "3),Vec2D(10, 20)).xx")
                  .state(),
              -1) == 2.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withScaleAndTranslation(Vec2D(2, "
                            "3),Vec2D(10, 20)).yy")
                  .state(),
              -1) == 3.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withScaleAndTranslation(Vec2D(2, "
                            "3),Vec2D(10, 20)).tx")
                  .state(),
              -1) == 10.0f);
    CHECK(lua_tonumber(
              ScriptingTest("return Mat2D.withScaleAndTranslation(Vec2D(2, "
                            "3),Vec2D(10, 20)).ty")
                  .state(),
              -1) == 20.0f);

    // scaleAndTranslation(xs, ys, tx, ty)
    CHECK(lua_tonumber(ScriptingTest(
                           "return Mat2D.withScaleAndTranslation(2,3,10,20).xx")
                           .state(),
                       -1) == 2.0f);
    CHECK(lua_tonumber(ScriptingTest(
                           "return Mat2D.withScaleAndTranslation(2,3,10,20).yy")
                           .state(),
                       -1) == 3.0f);
    CHECK(lua_tonumber(ScriptingTest(
                           "return Mat2D.withScaleAndTranslation(2,3,10,20).tx")
                           .state(),
                       -1) == 10.0f);
    CHECK(lua_tonumber(ScriptingTest(
                           "return Mat2D.withScaleAndTranslation(2,3,10,20).ty")
                           .state(),
                       -1) == 20.0f);
}

TEST_CASE("mat2d methods work", "[scripting]")
{
    CHECK(lua_tonumber(ScriptingTest("return Mat2D():invert().xx").state(),
                       -1) == 1.0f);

    CHECK(lua_isnil(ScriptingTest("return Mat2D(0,0,0,0,0,0):invert()").state(),
                    -1));

    CHECK(lua_toboolean(
              ScriptingTest("return Mat2D(0,0,0,0,0,0):isIdentity()").state(),
              -1) == 0);
    CHECK(lua_toboolean(
              ScriptingTest("return Mat2D(1,0,0,1,0,0):isIdentity()").state(),
              -1) == 1);
    CHECK(lua_toboolean(ScriptingTest("return Mat2D():isIdentity()").state(),
                        -1) == 1);
}

TEST_CASE("mat2d meta methods work", "[scripting]")
{
    CHECK(lua_toboolean(
              ScriptingTest("return Mat2D() == Mat2D(1,2,3,4,5,6)").state(),
              -1) == 0);

    CHECK(lua_toboolean(
              ScriptingTest("return Mat2D() ~= Mat2D(1,2,3,4,5,6)").state(),
              -1) == 1);

    CHECK(lua_toboolean(
              ScriptingTest("return Mat2D() == Mat2D(1,0,0,1,0,0)").state(),
              -1) == 1);

    CHECK(lua_toboolean(
              ScriptingTest("return Mat2D() ~= Mat2D(1,0,0,1,0,0)").state(),
              -1) == 0);

    CHECK(
        lua_tonumber(
            ScriptingTest("return (Mat2D.withScale(2) * Vec2D(1,1)).x").state(),
            -1) == 2);

    CHECK(
        lua_tonumber(
            ScriptingTest("return (Mat2D.withScale(2) * Mat2D.withScale(2)).xx")
                .state(),
            -1) == 4);
}
