#include <rive/file.hpp>
#include <rive/draw_rules.hpp>
#include <rive/draw_target.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include "utils/no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("draw rules load and sort correctly", "[draw rules]")
{
    auto file = ReadRiveFile("assets/draw_rule_cycle.riv");

    // auto file = reader.file();
    std::unique_ptr<rive::ArtboardInstance> artboard = file->artboardDefault();
    auto node = artboard->find<rive::Node>("Blue");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Shape>());
    // auto shape = node->as<rive::Shape>();

    artboard->updateComponents();
    REQUIRE(artboard->animationCount() == 1);

    // Check that we can advance the ping-pong animation with 1 second duration
    // without a hang.
    std::unique_ptr<rive::LinearAnimationInstance> animation =
        artboard->animationAt(0);
    // Advance and apply some frames.
    int frames = 10;
    float frameDuration = 1.0f;

    for (int i = 0; i < frames; i++)
    {
        animation->advanceAndApply(frameDuration);
        rive::NoOpRenderer renderer;
        artboard->draw(&renderer);
    }
}

TEST_CASE("draw targets without a drawable are dropped on load", "[draw rules]")
{
    // draw_rule_cycle.riv with drawableId stripped from every DrawTarget, which
    // is what the editor exports when a rule targets a drawable that doesn't
    // export (e.g. one in a guide layer). Used to crash Artboard::initialize.
    auto file = ReadRiveFile("assets/draw_target_missing_drawable.riv");

    std::unique_ptr<rive::ArtboardInstance> artboard = file->artboardDefault();
    REQUIRE(artboard->find<rive::DrawTarget>().empty());
    auto rules = artboard->find<rive::DrawRules>();
    REQUIRE(!rules.empty());
    for (auto rule : rules)
    {
        CHECK(rule->activeTarget() == nullptr);
    }

    std::unique_ptr<rive::LinearAnimationInstance> animation =
        artboard->animationAt(0);
    for (int i = 0; i < 10; i++)
    {
        animation->advanceAndApply(1.0f);
        rive::NoOpRenderer renderer;
        artboard->draw(&renderer);
    }
}
