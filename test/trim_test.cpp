#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/color.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("A 0 scale path will trim with no crash", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/trim.riv");

    auto artboard = file->artboard();
    auto node = artboard->find<rive::Node>("I");
    REQUIRE(node != nullptr);
    REQUIRE(node->scaleX() != 0);
    REQUIRE(node->scaleY() != 0);
    artboard->advance(0.0f);

    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);

    // Now set scale to 0 and make sure it doesn't crash.
    node->scaleX(0.0f);
    node->scaleY(0.0f);
    artboard->advance(0.0f);
    artboard->draw(&renderer);
}
