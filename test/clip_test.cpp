#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("clipping loads correctly", "[clipping]") {
    auto file = ReadRiveFile("../../test/assets/circle_clips.riv");

    auto node = file->artboard()->find("TopEllipse");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Shape>());

    auto shape = node->as<rive::Shape>();
    REQUIRE(shape->clippingShapes().size() == 2);
    REQUIRE(shape->clippingShapes()[0]->source()->name() == "ClipRect2");
    REQUIRE(shape->clippingShapes()[1]->source()->name() == "BabyEllipse");

    file->artboard()->updateComponents();

    rive::NoOpRenderer renderer;
    file->artboard()->draw(&renderer);
}
