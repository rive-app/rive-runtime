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
    auto file = ReadRiveFile("../../test/assets/background_measure.riv");

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
