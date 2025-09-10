#include "rive/file.hpp"
#include "rive/layout/n_sliced_node.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/node.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_value_run.hpp"
#include "utils/no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("compute bounds of background shape", "[bounds]")
{
    auto file = ReadRiveFile("assets/background_measure.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::Shape>("background") != nullptr);
    auto background = artboard->find<rive::Shape>("background");
    REQUIRE(artboard->find<rive::TextValueRun>("nameRun") != nullptr);
    auto name = artboard->find<rive::TextValueRun>("nameRun");
    artboard->advance(0.0f);

    auto bounds = background->computeWorldBounds();
    CHECK(bounds.width() == Approx(42.010925f));
    CHECK(bounds.height() == Approx(29.995453f));

    // Change the text and verify the bounds extended further.
    name->text("much much longer");
    artboard->advance(0.0f);

    bounds = background->computeWorldBounds();
    CHECK(bounds.width() == Approx(138.01093f));
    CHECK(bounds.height() == Approx(29.995453f));

    // Apply a transform to the whole artboard.
    rive::Mat2D& world = artboard->mutableWorldTransform();
    world.scaleByValues(0.5f, 0.5f);
    artboard->markWorldTransformDirty();
    artboard->advance(0.0f);

    bounds = background->computeWorldBounds();
    CHECK(bounds.width() == Approx(138.01093f / 2.0f));
    CHECK(bounds.height() == Approx(29.995453f / 2.0f));

    bounds = background->computeLocalBounds();
    CHECK(bounds.width() == Approx(138.01093f));
    CHECK(bounds.height() == Approx(29.995453f));
}

TEST_CASE("compute precise bounds of a raw path", "[bounds]")
{
    rive::RawPath path;
    path.moveTo(0.0f, 0.0f);
    path.cubicTo(236.0f, 10.0f, 569.0f, -58.0f, 366.0f, 180.0f);
    path.cubicTo(163.0f, 420.0f, 508.0f, 365.0f, 408.0f, 456.0f);
    path.cubicTo(308.0f, 547.0f, -236.0f, -10.0f, 0.0f, 0.0f);
    path.close();

    auto bounds = path.bounds();
    CHECK(bounds.left() == Approx(-236.0f));
    CHECK(bounds.top() == Approx(-58.0f));
    CHECK(bounds.right() == Approx(569.0f));
    CHECK(bounds.bottom() == Approx(547.0f));

    auto preciseBounds = path.preciseBounds();
    CHECK(preciseBounds.left() == Approx(-58.79769f));
    CHECK(preciseBounds.top() == Approx(-1.78456f));
    CHECK(preciseBounds.right() == Approx(428.90216f));
    CHECK(preciseBounds.bottom() == Approx(466.05313f));
}

TEST_CASE("test local bounds", "[bounds]")
{
    auto file = ReadRiveFile("assets/local_bounds.riv");

    auto artboard = file->artboard();

    REQUIRE(artboard->find<rive::Shape>("Shape1") != nullptr);
    REQUIRE(artboard->find<rive::Shape>("Shape2") != nullptr);
    REQUIRE(artboard->find<rive::Shape>("Shape3") != nullptr);
    REQUIRE(artboard->find<rive::Text>("Text1") != nullptr);
    REQUIRE(artboard->find<rive::Text>("Text2") != nullptr);
    REQUIRE(artboard->find<rive::Node>("Group1") != nullptr);
    REQUIRE(artboard->find<rive::Image>("Image1") != nullptr);
    REQUIRE(artboard->find<rive::Image>("Image1")->imageAsset() != nullptr);
    REQUIRE(artboard->find<rive::NSlicedNode>("NSlice2") != nullptr);
    REQUIRE(artboard->find<rive::Shape>("CustomShape1") != nullptr);
    REQUIRE(artboard->find<rive::Path>("CustomPath1") != nullptr);
    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutContainer") !=
            nullptr);
    REQUIRE(artboard->find<rive::LayoutComponent>("LayoutCellLeft") != nullptr);
    auto shape1 = artboard->find<rive::Shape>("Shape1");
    auto shape2 = artboard->find<rive::Shape>("Shape2");
    auto shape3 = artboard->find<rive::Shape>("Shape3");
    auto text1 = artboard->find<rive::Text>("Text1");
    auto text2 = artboard->find<rive::Text>("Text2");
    auto group1 = artboard->find<rive::Node>("Group1");
    auto image1 = artboard->find<rive::Image>("Image1");
    auto nslice2 = artboard->find<rive::NSlicedNode>("NSlice2");
    auto customShape1 = artboard->find<rive::Shape>("CustomShape1");
    auto customPath1 = artboard->find<rive::Path>("CustomPath1");
    auto layoutContainer =
        artboard->find<rive::LayoutComponent>("LayoutContainer");
    auto layoutCell = artboard->find<rive::LayoutComponent>("LayoutCellLeft");

    artboard->advance(0.0f);

    // Origin 0.5,0.5
    auto shape1Bounds = shape1->localBounds();
    CHECK(shape1Bounds.left() == -35.0f);
    CHECK(shape1Bounds.top() == -35.0f);
    CHECK(shape1Bounds.right() == 35.0f);
    CHECK(shape1Bounds.bottom() == 35.0f);
    // Origin 1.0,1.0
    auto shape2Bounds = shape2->localBounds();
    CHECK(shape2Bounds.left() == -80.0f);
    CHECK(shape2Bounds.top() == -80.0f);
    CHECK(shape2Bounds.right() == 0.0f);
    CHECK(shape2Bounds.bottom() == 0.0f);
    // Origin 0.0,0.0
    auto shape3Bounds = shape3->localBounds();
    CHECK(shape3Bounds.left() == 0.0f);
    CHECK(shape3Bounds.top() == 0.0f);
    CHECK(shape3Bounds.right() == 60.0f);
    CHECK(shape3Bounds.bottom() == 60.0f);
    // Origin 0.0,0.0
    auto text1Bounds = text1->localBounds();
    CHECK(text1Bounds.left() == 0.0f);
    CHECK(text1Bounds.top() == 0.0f);
    CHECK(text1Bounds.right() == Approx(159.55078f));
    CHECK(text1Bounds.bottom() == Approx(24.19921f));
    // Origin 0.5,0.5
    auto text2Bounds = text2->localBounds();
    CHECK(text2Bounds.left() == Approx(-79.77539f));
    CHECK(text2Bounds.top() == Approx(-12.099609f));
    CHECK(text2Bounds.right() == Approx(79.77539f));
    CHECK(text2Bounds.bottom() == Approx(12.099609f));

    auto group1Bounds = group1->localBounds();
    CHECK(group1Bounds.left() == 0.0f);
    CHECK(group1Bounds.top() == 0.0f);
    CHECK(group1Bounds.right() == 0.0f);
    CHECK(group1Bounds.bottom() == 0.0f);

    // Origin 0.5,0.5
    auto image1Bounds = image1->localBounds();
    CHECK(image1Bounds.left() == -64.0f);
    CHECK(image1Bounds.top() == -64.0f);
    CHECK(image1Bounds.right() == 64.0f);
    CHECK(image1Bounds.bottom() == 64.0f);

    auto nslice2Bounds = nslice2->localBounds();
    CHECK(nslice2Bounds.left() == 0.0f);
    CHECK(nslice2Bounds.top() == 0.0f);
    CHECK(nslice2Bounds.right() == Approx(112.1891f));
    CHECK(nslice2Bounds.bottom() == Approx(77.7086f));

    auto customShape1Bounds = customShape1->localBounds();
    CHECK(customShape1Bounds.left() == Approx(-27.82596f));
    CHECK(customShape1Bounds.top() == Approx(-32.0276f));
    CHECK(customShape1Bounds.right() == Approx(105.36988f));
    CHECK(customShape1Bounds.bottom() == Approx(52.38258f));

    auto customPath1Bounds = customPath1->localBounds();
    CHECK(customPath1Bounds.left() == Approx(-11.52589f));
    CHECK(customPath1Bounds.top() == Approx(-25.32601f));
    CHECK(customPath1Bounds.right() == Approx(100.66321f));
    CHECK(customPath1Bounds.bottom() == Approx(52.38258f));

    auto layoutContainerBounds = layoutContainer->localBounds();
    CHECK(layoutContainerBounds.left() == 0.0f);
    CHECK(layoutContainerBounds.top() == 0.0f);
    CHECK(layoutContainerBounds.right() == 200.0f);
    CHECK(layoutContainerBounds.bottom() == 100.0f);

    auto layoutCellBounds = layoutCell->localBounds();
    CHECK(layoutCellBounds.left() == 0.0f);
    CHECK(layoutCellBounds.top() == 0.0f);
    CHECK(layoutCellBounds.right() == 88.0f);
    CHECK(layoutCellBounds.bottom() == 84.0f);
}