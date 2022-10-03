#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("cloning an ellipse works", "[instancing]")
{
    auto file = ReadRiveFile("../../test/assets/circle_clips.riv");

    auto node = file->artboard()->find<rive::Shape>("TopEllipse");
    REQUIRE(node != nullptr);

    auto clonedNode = node->clone()->as<rive::Shape>();
    REQUIRE(node->x() == clonedNode->x());
    REQUIRE(node->y() == clonedNode->y());

    delete clonedNode;
}

TEST_CASE("instancing artboard clones clipped properties", "[instancing]")
{
    auto file = ReadRiveFile("../../test/assets/circle_clips.riv");

    REQUIRE(!file->artboard()->isInstance());

    auto artboard = file->artboardDefault();

    REQUIRE(artboard->isInstance());

    auto node = artboard->find("TopEllipse");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Shape>());

    auto shape = node->as<rive::Shape>();
    REQUIRE(shape->clippingShapes().size() == 2);
    REQUIRE(shape->clippingShapes()[0]->source()->name() == "ClipRect2");
    REQUIRE(shape->clippingShapes()[1]->source()->name() == "BabyEllipse");

    artboard->updateComponents();

    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
}

TEST_CASE("instancing artboard doesn't clone animations", "[instancing]")
{
    auto file = ReadRiveFile("../../test/assets/juice.riv");

    auto artboard = file->artboardDefault();

    REQUIRE(file->artboard()->animationCount() == artboard->animationCount());
    REQUIRE(file->artboard()->firstAnimation() == artboard->firstAnimation());

    rive::LinearAnimation::deleteCount = 0;
    // Make sure no animations were deleted by deleting the instance.
    REQUIRE(rive::LinearAnimation::deleteCount == 0);

    size_t numberOfAnimations = file->artboard()->animationCount();
    file.reset(nullptr);
    // Now the animations should've been deleted.
    REQUIRE(rive::LinearAnimation::deleteCount == numberOfAnimations);
}
