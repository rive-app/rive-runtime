#include <rive/file.hpp>
#include <rive/layout_component.hpp>
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
  auto file = ReadRiveFile("assets/circle_clips.riv");

  auto node = file->artboard()->find<rive::Shape>("TopEllipse");
  REQUIRE(node != nullptr);

  auto clonedNode = node->clone()->as<rive::Shape>();
  REQUIRE(node->x() == clonedNode->x());
  REQUIRE(node->y() == clonedNode->y());

  delete clonedNode;
}

TEST_CASE("instancing artboard clones clipped properties", "[instancing]")
{
  auto file = ReadRiveFile("assets/circle_clips.riv");

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
  auto file = ReadRiveFile("assets/juice.riv");

  auto artboard = file->artboardDefault();

  REQUIRE(file->artboard()->animationCount() == artboard->animationCount());
  REQUIRE(file->artboard()->firstAnimation() == artboard->firstAnimation());

  rive::LinearAnimation::deleteCount = 0;
  // Make sure no animations were deleted by deleting the instance.
  REQUIRE(rive::LinearAnimation::deleteCount == 0);

  size_t numberOfAnimations = file->artboard()->animationCount();
  artboard.reset(nullptr);
  file.reset(nullptr);
  // Now the animations should've been deleted.
  REQUIRE(rive::LinearAnimation::deleteCount == numberOfAnimations);
}

TEST_CASE("an instance's lists are sized for its source's", "[instancing]")
{
  // Added one entry at a time, three children grow the list to room for
  // four and twelve dependents to room for sixteen; sized from the source,
  // each holds exactly what it ends up with.
  auto rowFile = ReadRiveFile("assets/component_list_rtl_host.riv");
  auto rowSource = rowFile->artboardNamed("Row");
  REQUIRE(rowSource != nullptr);
  auto row = rowSource->instance();
  REQUIRE(row != nullptr);
  REQUIRE(row->children().size() == rowSource->children().size());
  REQUIRE(row->children().size() == 3);
  CHECK(row->children().capacity() == 3);

  auto clipsFile = ReadRiveFile("assets/circle_clips.riv");
  auto clipsSource = clipsFile->artboard();
  REQUIRE(clipsSource != nullptr);
  auto clips = clipsSource->instance();
  REQUIRE(clips != nullptr);
  REQUIRE(clips->dependents().size() == clipsSource->dependents().size());
  REQUIRE(clips->dependents().size() == 12);
  CHECK(clips->dependents().capacity() == 12);
}
