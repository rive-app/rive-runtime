#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/paint/shape_paint.hpp>
#include "rive_file_reader.hpp"

TEST_CASE("Nested artboard background renders with opacity", "[file]")
{
    auto file = ReadRiveFile("../../test/assets/nested_artboard_opacity.riv");

    auto mainArtboard = file->artboard()->instance();
    REQUIRE(mainArtboard->find("Parent Artboard") != nullptr);
    auto artboard = mainArtboard->find<rive::Artboard>("Parent Artboard");
    artboard->updateComponents();
    REQUIRE(artboard->is<rive::Artboard>());
    REQUIRE(artboard->find("Nested artboard container") != nullptr);
    auto nestedArtboardContainer =
        artboard->find<rive::NestedArtboard>("Nested artboard container");
    REQUIRE(nestedArtboardContainer->artboard() != nullptr);
    auto nestedArtboard = nestedArtboardContainer->artboard();
    nestedArtboard->updateComponents();
    auto paints = nestedArtboard->shapePaints();
    REQUIRE(paints.size() == 1);
    auto paint = paints[0];
    REQUIRE(paint->is<rive::ShapePaint>());
    REQUIRE(paint->renderOpacity() == 0.3275f);
}
