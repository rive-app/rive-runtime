#include <rive/artboard.hpp>
#include <rive/animation/linear_animation.hpp>
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "rive/shapes/shape.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("LinearAnimation with positive speed have normal start and end seconds", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->speed(1);

    REQUIRE(linearAnimation->startSeconds() == 0.0);
    REQUIRE(linearAnimation->endSeconds() == 5.0);

    REQUIRE(linearAnimation->startTime() == 0.0);
    REQUIRE(linearAnimation->endTime() == 5.0);
    REQUIRE(linearAnimation->durationSeconds() == 5.0);

    delete linearAnimation;
}

TEST_CASE("LinearAnimation with negative speed have reversed start and end seconds", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->speed(-1);

    REQUIRE(linearAnimation->startSeconds() == 0.0);
    REQUIRE(linearAnimation->endSeconds() == 5.0);
    REQUIRE(linearAnimation->startTime() == 5.0);
    REQUIRE(linearAnimation->endTime() == 0.0);

    REQUIRE(linearAnimation->durationSeconds() == 5.0);

    delete linearAnimation;
}

TEST_CASE("quantize goes to whole frames", "[animation]")
{
    auto file = ReadRiveFile("../../test/assets/quantize_test.riv");
    auto artboard = file->artboard();
    auto animation = artboard->animation(0);
    REQUIRE(animation->quantize());

    auto shapes = artboard->find<rive::Shape>();
    REQUIRE(shapes.size() == 1);
    auto ellipse = shapes[0];

    animation->apply(artboard, 0.0f);
    REQUIRE(ellipse->x() == 0.0f);

    // Animation has 5 frames and lasts 1 second. So going to 0.5 seconds should
    // show frame 2.5 which should floor to frame 2's value of 160 on x. Without
    // quantize this would be 200.
    animation->apply(artboard, 0.5f);
    REQUIRE(ellipse->x() == 160.0f);

    // Make sure our assumption with quantize off is true.
    animation->quantize(false);
    animation->apply(artboard, 0.5f);
    REQUIRE(ellipse->x() == 200.0f);
}