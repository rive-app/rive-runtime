#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/animation/cubic_value_interpolator.hpp>
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>

TEST_CASE("test cubic value load and interpolate properly", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/cubic_value_test.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);

    auto greyRect = artboard->find<rive::Node>("grey_rectangle");
    REQUIRE(greyRect != nullptr);

    REQUIRE(artboard->find<rive::CubicValueInterpolatorBase>().size() == 3);

    auto animation = artboard->animation("Timeline 1");
    REQUIRE(animation != nullptr);
    // Go to frame 15.
    animation->apply(artboard, 15.0f / animation->fps(), 1.0f);
    REQUIRE(greyRect->x() == Approx(290.71f));

    // Go to frame 11.
    animation->apply(artboard, 11.0f / animation->fps(), 1.0f);
    REQUIRE(greyRect->x() == Approx(363.01f));
}
