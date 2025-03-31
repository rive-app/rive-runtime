#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("juice silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/juice.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto walkAnimation = artboard->animationNamed("walk");
    REQUIRE(walkAnimation != nullptr);

    auto renderer = silver.makeRenderer();
    // Draw first frame.
    walkAnimation->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    int frames = (int)(walkAnimation->durationSeconds() / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        walkAnimation->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("juice"));
}

TEST_CASE("n-slice silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/n_slice_triangle.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());
    artboard->advance(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("n_slice_triangle"));
}
