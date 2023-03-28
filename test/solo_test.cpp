#include <rive/solo.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("children load correclty", "[solo]")
{
    auto file = ReadRiveFile("../../test/assets/solo_test.riv");

    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 1);
    auto solo = solos[0];
    REQUIRE(solo != nullptr);
    REQUIRE(solo->children().size() == 3);
    REQUIRE(solo->children()[0]->is<rive::Shape>());
    REQUIRE(solo->children()[0]->name() == "Blue");
    REQUIRE(solo->children()[1]->is<rive::Shape>());
    REQUIRE(solo->children()[1]->name() == "Green");
    REQUIRE(solo->children()[2]->is<rive::Shape>());
    REQUIRE(solo->children()[2]->name() == "Red");

    auto blue = solo->children()[0]->as<rive::Shape>();
    auto green = solo->children()[1]->as<rive::Shape>();
    auto red = solo->children()[2]->as<rive::Shape>();

    REQUIRE(!blue->isHidden());
    REQUIRE(green->isHidden());
    REQUIRE(red->isHidden());

    REQUIRE(green->children().size() == 2);
    REQUIRE(green->children()[0]->isCollapsed());
    REQUIRE(green->children()[1]->isCollapsed());

    REQUIRE(red->children().size() == 2);
    REQUIRE(red->children()[0]->isCollapsed());
    REQUIRE(red->children()[1]->isCollapsed());

    auto machine = artboard->defaultStateMachine();
    machine->advanceAndApply(0.0);
    // Red visible at start
    REQUIRE(blue->isHidden());
    REQUIRE(green->isHidden());
    REQUIRE(!red->isHidden());

    machine->advanceAndApply(0.5);
    // Green visible after 0.5 seconds.
    REQUIRE(blue->isHidden());
    REQUIRE(!green->isHidden());
    REQUIRE(red->isHidden());

    machine->advanceAndApply(0.5);
    // Blue visible at end
    REQUIRE(!blue->isHidden());
    REQUIRE(green->isHidden());
    REQUIRE(red->isHidden());
}
