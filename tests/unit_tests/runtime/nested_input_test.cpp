#include "rive/core/binary_reader.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>

TEST_CASE("validate nested boolean get/set", "[nestedInput]")
{
    auto file = ReadRiveFile("../assets/runtime_nested_inputs.riv");

    auto artboard = file->artboard("MainArtboard")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    // Test getting/setting boolean SMIInput via nested artboard path
    auto boolInput = artboard->getBool("CircleOuterState", "CircleOuter");
    auto smiInput = artboard->input("CircleOuterState", "CircleOuter");
    auto smiBoolInput = static_cast<rive::SMIBool*>(smiInput);
    auto nestedArtboard = artboard->nestedArtboard("CircleOuter");
    auto nestedInput = nestedArtboard->input("CircleOuterState")->as<rive::NestedBool>();
    REQUIRE(boolInput->value() == false);
    REQUIRE(smiBoolInput->value() == false);
    REQUIRE(nestedInput->nestedValue() == false);

    boolInput->value(true);
    REQUIRE(boolInput->value() == true);
    REQUIRE(smiBoolInput->value() == true);
    REQUIRE(nestedInput->nestedValue() == true);

    smiBoolInput->value(false);
    REQUIRE(boolInput->value() == false);
    REQUIRE(smiBoolInput->value() == false);
    REQUIRE(nestedInput->nestedValue() == false);

    nestedInput->nestedValue(true);
    REQUIRE(boolInput->value() == true);
    REQUIRE(smiBoolInput->value() == true);
    REQUIRE(nestedInput->nestedValue() == true);
}

TEST_CASE("validate nested number get/set", "[nestedInput]")
{
    auto file = ReadRiveFile("../assets/runtime_nested_inputs.riv");

    auto artboard = file->artboard("MainArtboard")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    // Test getting/setting number SMIInput via nested artboard path
    auto numInput = artboard->getNumber("CircleOuterNumber", "CircleOuter");
    auto smiInput = artboard->input("CircleOuterNumber", "CircleOuter");
    auto smiNumInput = static_cast<rive::SMINumber*>(smiInput);
    auto nestedArtboard = artboard->nestedArtboardAtPath("CircleOuter");
    auto nestedInput = nestedArtboard->input("CircleOuterNumber")->as<rive::NestedNumber>();
    REQUIRE(numInput->value() == 0);
    REQUIRE(smiNumInput->value() == 0);
    REQUIRE(nestedInput->nestedValue() == 0);

    numInput->value(10);
    REQUIRE(numInput->value() == 10);
    REQUIRE(smiNumInput->value() == 10);
    REQUIRE(nestedInput->nestedValue() == 10);

    smiNumInput->value(5);
    REQUIRE(numInput->value() == 5);
    REQUIRE(smiNumInput->value() == 5);
    REQUIRE(nestedInput->nestedValue() == 5);

    nestedInput->nestedValue(99);
    REQUIRE(numInput->value() == 99);
    REQUIRE(smiNumInput->value() == 99);
    REQUIRE(nestedInput->nestedValue() == 99);
}

TEST_CASE("validate nested trigger fire", "[nestedInput]")
{
    auto file = ReadRiveFile("../assets/runtime_nested_inputs.riv");

    auto artboard = file->artboard("MainArtboard")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    // Test getting/setting number SMIInput via nested artboard path
    auto tInput = artboard->getTrigger("CircleOuterTrigger", "CircleOuter");
    auto smiInput = artboard->input("CircleOuterTrigger", "CircleOuter");
    auto smiTInput = static_cast<rive::SMITrigger*>(smiInput);
    auto nestedArtboard = artboard->nestedArtboardAtPath("CircleOuter");
    auto nestedInput = nestedArtboard->input("CircleOuterTrigger")->as<rive::NestedTrigger>();
    auto nestedSMI = static_cast<rive::SMITrigger*>(nestedInput->input());
    REQUIRE(tInput->didFire() == false);
    REQUIRE(smiTInput->didFire() == false);
    REQUIRE(nestedSMI->didFire() == false);

    tInput->fire();
    REQUIRE(tInput->didFire() == true);
    REQUIRE(smiTInput->didFire() == true);
    REQUIRE(nestedSMI->didFire() == true);
}

TEST_CASE("validate nested boolean get/set multiple nested artboards deep", "[nestedInput]")
{
    auto file = ReadRiveFile("../assets/runtime_nested_inputs.riv");

    auto artboard = file->artboard("MainArtboard")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    // Test getting/setting boolean SMIInput via nested artboard path
    auto boolInput = artboard->getBool("CircleInnerState", "CircleOuter/CircleInner");
    auto smiInput = artboard->input("CircleInnerState", "CircleOuter/CircleInner");
    auto smiBoolInput = static_cast<rive::SMIBool*>(smiInput);
    auto nestedArtboard = artboard->nestedArtboardAtPath("CircleOuter/CircleInner");
    auto nestedInput = nestedArtboard->input("CircleInnerState")->as<rive::NestedBool>();
    REQUIRE(boolInput->value() == false);
    REQUIRE(smiBoolInput->value() == false);
    REQUIRE(nestedInput->nestedValue() == false);

    boolInput->value(true);
    REQUIRE(boolInput->value() == true);
    REQUIRE(smiBoolInput->value() == true);
    REQUIRE(nestedInput->nestedValue() == true);

    smiBoolInput->value(false);
    REQUIRE(boolInput->value() == false);
    REQUIRE(smiBoolInput->value() == false);
    REQUIRE(nestedInput->nestedValue() == false);

    nestedInput->nestedValue(true);
    REQUIRE(boolInput->value() == true);
    REQUIRE(smiBoolInput->value() == true);
    REQUIRE(nestedInput->nestedValue() == true);
}