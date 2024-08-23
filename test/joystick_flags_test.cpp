#include "rive/joystick.hpp"
#include "rive/shapes/shape.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"

TEST_CASE("joystick flags load as expected", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/joystick_flag_test.riv");
    auto artboard = file->artboard();

    auto invertX = artboard->find<rive::Joystick>("Invert X Joystick");
    REQUIRE(invertX->isJoystickFlagged(rive::JoystickFlags::invertX));
    REQUIRE(!invertX->isJoystickFlagged(rive::JoystickFlags::invertY));
    REQUIRE(!invertX->isJoystickFlagged(rive::JoystickFlags::worldSpace));

    invertX->x(0.0f);
    invertX->apply(artboard);
    REQUIRE(artboard->find<rive::Shape>("invert_x_rect")->x() == 350.0f);

    invertX->x(1.0f);
    invertX->apply(artboard);
    REQUIRE(artboard->find<rive::Shape>("invert_x_rect")->x() == 300.0f);

    invertX->x(-1.0f);
    invertX->apply(artboard);
    REQUIRE(artboard->find<rive::Shape>("invert_x_rect")->x() == 400.0f);

    auto invertY = artboard->find<rive::Joystick>("Invert Y Joystick");
    REQUIRE(!invertY->isJoystickFlagged(rive::JoystickFlags::invertX));
    REQUIRE(invertY->isJoystickFlagged(rive::JoystickFlags::invertY));
    REQUIRE(!invertY->isJoystickFlagged(rive::JoystickFlags::worldSpace));

    invertY->y(0.0f);
    invertY->apply(artboard);
    REQUIRE(artboard->find<rive::Shape>("invert_y_ellipse")->x() == 425.0f);

    invertY->y(1.0f);
    invertY->apply(artboard);
    REQUIRE(artboard->find<rive::Shape>("invert_y_ellipse")->x() == 400.0f);

    invertY->y(-1.0f);
    invertY->apply(artboard);
    REQUIRE(artboard->find<rive::Shape>("invert_y_ellipse")->x() == 450.0f);

    auto world = artboard->find<rive::Joystick>("World Joystick");
    REQUIRE(!world->isJoystickFlagged(rive::JoystickFlags::invertX));
    REQUIRE(!world->isJoystickFlagged(rive::JoystickFlags::invertY));
    REQUIRE(world->isJoystickFlagged(rive::JoystickFlags::worldSpace));

    auto normal = artboard->find<rive::Joystick>("Normal Joystick");
    REQUIRE(!normal->isJoystickFlagged(rive::JoystickFlags::invertX));
    REQUIRE(!normal->isJoystickFlagged(rive::JoystickFlags::invertY));
    REQUIRE(!normal->isJoystickFlagged(rive::JoystickFlags::worldSpace));
}