#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/paint/shape_paint.hpp>
#include "rive_file_reader.hpp"

TEST_CASE("Nested artboard background renders with opacity", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");

    auto mainArtboard = file->artboard()->instance();
    REQUIRE(mainArtboard->find("Parent Artboard") != nullptr);
    auto artboard = mainArtboard->find<rive::Artboard>("Parent Artboard");
    artboard->updateComponents();
    REQUIRE(artboard->is<rive::Artboard>());
    REQUIRE(artboard->find("Nested artboard container") != nullptr);
    auto nestedArtboardContainer =
        artboard->find<rive::NestedArtboard>("Nested artboard container");
    REQUIRE(nestedArtboardContainer->artboardInstance() != nullptr);
    auto nestedArtboard = nestedArtboardContainer->artboardInstance();
    nestedArtboard->updateComponents();
    auto paints = nestedArtboard->shapePaints();
    REQUIRE(paints.size() == 1);
    auto paint = paints[0];
    REQUIRE(paint->is<rive::ShapePaint>());
    REQUIRE(paint->renderOpacity() == 0.3275f);
}

// A paused nested artboard skips its advance path, so it never lifts Components
// dirt onto the host. A host opacity change must still propagate down to the
// nested artboard's paints via the (advance-independent) update path.
TEST_CASE("Paused nested artboard still propagates host opacity", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");

    auto mainArtboard = file->artboard()->instance();
    auto artboard = mainArtboard->find<rive::Artboard>("Parent Artboard");
    REQUIRE(artboard != nullptr);
    auto container =
        artboard->find<rive::NestedArtboard>("Nested artboard container");
    REQUIRE(container != nullptr);
    REQUIRE(container->artboardInstance() != nullptr);

    // Settle the tree once (playing) to establish a baseline.
    artboard->advance(0.0f);
    auto paints = container->artboardInstance()->shapePaints();
    REQUIRE(paints.size() == 1);
    auto paint = paints[0];
    float baseline = paint->renderOpacity();
    REQUIRE(baseline > 0.0f);

    // Pause the nested artboard (its advance path now short-circuits), then
    // halve the host opacity. The change must still reach the nested paint.
    container->isPaused(true);
    container->opacity(container->opacity() * 0.5f);
    artboard->advance(0.0f);

    REQUIRE(paint->renderOpacity() == Approx(baseline * 0.5f));
}

// The host (NestedArtboard) imposes its opacity through hostOpacity() rather
// than overwriting the instance's own `opacity` property, so an artboard's own
// (authored / animated / data-bound) opacity is honored and multiplied with the
// host opacity instead of being discarded.
TEST_CASE("Nested artboard's own opacity combines with host opacity", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");

    auto mainArtboard = file->artboard()->instance();
    auto artboard = mainArtboard->find<rive::Artboard>("Parent Artboard");
    artboard->updateComponents();
    auto nestedArtboardContainer =
        artboard->find<rive::NestedArtboard>("Nested artboard container");
    auto nestedArtboard = nestedArtboardContainer->artboardInstance();

    // Give the mounted instance its own opacity, as animation / data binding
    // would, and impose a separate host opacity as the container does.
    nestedArtboard->opacity(0.4f);
    nestedArtboard->hostOpacity(0.5f);
    nestedArtboard->updateComponents();

    // The own opacity property survives (not clobbered by the host)...
    REQUIRE(nestedArtboard->opacity() == 0.4f);
    // ...and contents receive the product of own and host opacity.
    REQUIRE(nestedArtboard->childOpacity() == Approx(0.2f));
    auto paint = nestedArtboard->shapePaints()[0];
    // The background paint (own opacity 1.0) receives the combined opacity.
    REQUIRE(paint->renderOpacity() == Approx(0.2f));
}
