#include "rive/file.hpp"
#include "rive/node.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/math/transform_components.hpp"
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