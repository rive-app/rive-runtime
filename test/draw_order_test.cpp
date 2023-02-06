#include <rive/file.hpp>
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
    auto file = ReadRiveFile("../../test/assets/draw_rule_cycle.riv");

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
    std::unique_ptr<rive::LinearAnimationInstance> animation = artboard->animationAt(0);
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
