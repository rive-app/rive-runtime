#include <rive/artboard.hpp>
#include <rive/animation/linear_animation.hpp>
#include "utils/no_op_factory.hpp"
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
