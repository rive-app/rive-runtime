
#include "catch.hpp"
#include "lua.h"
#include "scripting_test_utilities.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"

using namespace rive;

TEST_CASE("color can be constructed", "[scripting]")
{
    CHECK(lua_tounsigned(
              ScriptingTest("return Color.rgba(255, 255, 255, 255)").state(),
              -1) == 0xFFFFFFFF);

    CHECK(lua_tounsigned(
              ScriptingTest("return Color.rgba(255, 255, 0, 255)").state(),
              -1) == 0xFFFFFF00);

    CHECK(lua_tounsigned(ScriptingTest("return Color.rgb(255, 0, 0)").state(),
                         -1) == 0xFFFF0000);
}

TEST_CASE("color static methods work", "[scripting]")
{
    // get red color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "return Color.red(color)")
                  .state(),
              -1) == 225);

    // set red color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "color = Color.red(color, 129)"
                            "return Color.red(color)")
                  .state(),
              -1) == 129);

    // get green color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "return Color.green(color)")
                  .state(),
              -1) == 48);

    // set green color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "color = Color.green(color, 129)"
                            "return Color.green(color)")
                  .state(),
              -1) == 129);

    // get blue color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "return Color.blue(color)")
                  .state(),
              -1) == 108);

    // set blue color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "color = Color.blue(color, 129)"
                            "return Color.blue(color)")
                  .state(),
              -1) == 129);

    // get alpha color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "return Color.alpha(color)")
                  .state(),
              -1) == 255);

    // set alpha color component
    CHECK(lua_tounsigned(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "color = Color.alpha(color, 129)"
                            "return Color.alpha(color)")
                  .state(),
              -1) == 129);

    // get opacity color component
    CHECK(lua_tonumber(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "return Color.opacity(color)")
                  .state(),
              -1) == 1.0);

    // set opacity color component
    CHECK(lua_tonumber(
              ScriptingTest("local color = Color.rgba(225, 48, 108, 255)\n"
                            "color = Color.opacity(color, 0.6)"
                            "return Color.opacity(color)")
                  .state(),
              -1) == Approx(0.6));

    // lerp colors
    CHECK(lua_tounsigned(ScriptingTest("local black = Color.rgb(0,0,0)\n"
                                       "local white = Color.rgb(255,255,255)\n"
                                       "return Color.lerp(black, white, 0.5)")
                             .state(),
                         -1) == 0xff808080);
}

TEST_CASE("Scripted color converter", "[scripting]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_color_data_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("script_color_data_test"));
}