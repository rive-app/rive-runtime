#include <rive/animation/loop.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include "utils/no_op_factory.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("LinearAnimationInstance oneShot", "[animation]")
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
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());

    // play from beginning.
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    // get stuck at end
    continuePlaying = linearAnimationInstance->advance(10.0);
    REQUIRE(linearAnimationInstance->time() == 5.0);
    REQUIRE(linearAnimationInstance->totalTime() == 12.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);
    REQUIRE(continuePlaying == false);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance speed", "[animation]")
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
    linearAnimation->speed(.5);

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());

    // play from beginning.
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 1.0f);
    REQUIRE(linearAnimationInstance->totalTime() == 1.0f);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance negative advance adds absolute total time ", "[animation]")
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
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());

    // play from beginning.
    bool continuePlaying = linearAnimationInstance->advance(-2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 3.0f);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0f);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance oneShot <-", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());
    linearAnimationInstance->direction(-1);
    REQUIRE(linearAnimationInstance->time() == 0.0);

    // Advancing 2 seconds backwards will keep the "animation" time at 0
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == false);
    REQUIRE(linearAnimationInstance->time() == 0.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    // set time to end..
    // TODO: this also "resets" the total time. is that sensible?
    linearAnimationInstance->time(5.0);
    REQUIRE(linearAnimationInstance->totalTime() == 5.0);
    // TODO: get rid if we stop killing m_Direction
    linearAnimationInstance->direction(-1);

    // play from end to beginning
    continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 3.0);
    REQUIRE(linearAnimationInstance->totalTime() == 7.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    // get stuck at beginning
    continuePlaying = linearAnimationInstance->advance(4.0);
    REQUIRE(continuePlaying == false);
    REQUIRE(linearAnimationInstance->time() == 0.0);
    REQUIRE(linearAnimationInstance->totalTime() == 11.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance loop ->", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());

    // play from beginning.
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    // loop around a couple of times, back to the same spot
    continuePlaying = linearAnimationInstance->advance(10.0);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 12.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);
    REQUIRE(continuePlaying == true);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance loop <-", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());
    linearAnimationInstance->direction(-1);
    REQUIRE(linearAnimationInstance->time() == 0.0);

    // Advancing 2 seconds backwards will get the "animation" time to 3
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->time() == 3.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    // play without looping past the beginning.
    continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 1.0);
    REQUIRE(linearAnimationInstance->totalTime() == 4.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    // loop past beginning again
    continuePlaying = linearAnimationInstance->advance(4.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 8.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance loop <- work area", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 50
    linearAnimation->workStart(4);
    linearAnimation->enableWorkArea(true);
    linearAnimation->workEnd(10);
    linearAnimation->duration(100);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());
    linearAnimationInstance->direction(-1);
    REQUIRE(linearAnimationInstance->time() == 2.0);

    // advancing by 0 will not change anything
    // is expected to return continue playing true regardless
    bool continuePlaying = linearAnimationInstance->advance(0.0);

    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 0.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    // 2 more secs , 5s -> 3s
    continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->time() == 3.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    // 2 more secs , 3s -> 1s, thats before start, so loops to 4s
    continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->time() == 4.0);
    REQUIRE(linearAnimationInstance->totalTime() == 4.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    // another hit, 4->2s, thats at the start, loops to 5s
    continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 5.0);
    REQUIRE(linearAnimationInstance->totalTime() == 6.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance pingpong ->", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::pingPong));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());

    // play from beginning.
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    // pingpong at the end and come back to 3.
    continuePlaying = linearAnimationInstance->advance(5.0);
    REQUIRE(linearAnimationInstance->time() == 3.0);
    REQUIRE(linearAnimationInstance->totalTime() == 7.0);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->didLoop() == true);
    REQUIRE(continuePlaying == true);
    // pingpong at the start and come back to 6.
    continuePlaying = linearAnimationInstance->advance(9.0);
    REQUIRE(linearAnimationInstance->time() == 4.0);
    REQUIRE(linearAnimationInstance->totalTime() == 16.0);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->didLoop() == true);
    REQUIRE(continuePlaying == true);

    // pingpong back around to 2
    continuePlaying = linearAnimationInstance->advance(6.0);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 22.0);
    REQUIRE(linearAnimationInstance->direction() == 1);
    REQUIRE(linearAnimationInstance->didLoop() == true);
    REQUIRE(continuePlaying == true);

    // Loop around twice in a frame.
    continuePlaying = linearAnimationInstance->advance(20.0);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 42.0);
    REQUIRE(linearAnimationInstance->direction() == 1);
    REQUIRE(linearAnimationInstance->didLoop() == true);
    REQUIRE(continuePlaying == true);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance pingpong <-", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::pingPong));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());
    linearAnimationInstance->direction(-1);
    REQUIRE(linearAnimationInstance->time() == 0.0);

    // Advancing 2 seconds backwards, pongs immediately (and is acutally just
    // like playing forwards)
    bool continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == 1);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 2.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    // pingpong at the end
    continuePlaying = linearAnimationInstance->advance(4.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->time() == 4.0);
    REQUIRE(linearAnimationInstance->totalTime() == 6.0);
    REQUIRE(linearAnimationInstance->didLoop() == true);

    // just a normal advance, no loop
    continuePlaying = linearAnimationInstance->advance(2.0);
    REQUIRE(continuePlaying == true);
    REQUIRE(linearAnimationInstance->direction() == -1);
    REQUIRE(linearAnimationInstance->time() == 2.0);
    REQUIRE(linearAnimationInstance->totalTime() == 8.0);
    REQUIRE(linearAnimationInstance->didLoop() == false);

    delete linearAnimationInstance;
    delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance override loop", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::LinearAnimationInstance* linearAnimationInstance =
        new rive::LinearAnimationInstance(linearAnimation, abi.get());

    // Check the loop value is same as the animation's
    REQUIRE(linearAnimationInstance->loopValue() == linearAnimation->loopValue());

    // Override the loop type
    linearAnimationInstance->loopValue(static_cast<int>(rive::Loop::pingPong));
    REQUIRE(linearAnimationInstance->loopValue() != linearAnimation->loopValue());
    REQUIRE(linearAnimationInstance->loopValue() == static_cast<int>(rive::Loop::pingPong));
    REQUIRE(linearAnimationInstance->loop() == rive::Loop::pingPong);

    delete linearAnimationInstance;
    delete linearAnimation;
}
