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

TEST_CASE("stroke can be looked up at runtime", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/stroke_name_test.riv");

    auto artboard = file->artboard();
    REQUIRE(artboard->find<rive::Stroke>("white_stroke") != nullptr);
    auto stroke = artboard->find<rive::Stroke>("white_stroke");
    REQUIRE(stroke->paint()->is<rive::SolidColor>());
    stroke->paint()->as<rive::SolidColor>()->colorValue(rive::colorARGB(255, 0, 255, 255));
}
