#include "rive/animation/loop.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"

#include "rive/animation/animation_state.hpp"
#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "utils/no_op_factory.hpp"
#include "rive/scene.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("AnimationStateInstance advances in step with animation speed 1",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());
    animationStateInstance->advance(2.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 2.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 2.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance advances twice as fast when speed is doubled",
          "[animation]")
{
    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 5
    linearAnimation->duration(10);
    linearAnimation->fps(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);
    animationState->speed(2);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(2.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 4.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 4.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance advances half as fast when speed is halved",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);
    animationState->speed(0.5);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());
    animationStateInstance->advance(2.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 1.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 1.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance advances backwards when speed is negative",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);
    animationState->speed(-1);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());
    animationStateInstance->advance(2.0, &stateMachineInstance);

    // backwards 2 seconds from 5.
    REQUIRE(animationStateInstance->animationInstance()->time() == 3.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 2.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance starts a positive animation at the beginning",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // backwards 2 seconds from 5.
    REQUIRE(animationStateInstance->animationInstance()->time() == 0.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance starts a negative animation at the end",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // backwards 2 seconds from 5.
    REQUIRE(animationStateInstance->animationInstance()->time() == 5.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance with negative speed starts a positive "
          "animation at the end",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->speed(-1);
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // backwards 2 seconds from 5.
    REQUIRE(animationStateInstance->animationInstance()->time() == 5.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance with negative speed starts a negative "
          "animation at the beginning",
          "[animation]")
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

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->speed(-1);
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // backwards 2 seconds from 5.
    REQUIRE(animationStateInstance->animationInstance()->time() == 0.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE(
    "AnimationStateInstance spilledTime accounts for Nx speed with oneShot",
    "[animation]")
{

    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 2
    linearAnimation->duration(4);
    linearAnimation->fps(2);
    linearAnimation->speed(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(3.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 2.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 6.0);
    // Duration is 2s but at a 2x speed it takes 1s to end
    // When advancing 3s, there are still 2s remaining (spilled)
    REQUIRE(animationStateInstance->animationInstance()->spilledTime() == 2.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE(
    "AnimationStateInstance spilledTime accounts for 1/Nx speed with oneShot",
    "[animation]")
{

    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 2
    linearAnimation->duration(4);
    linearAnimation->fps(2);
    linearAnimation->speed(0.5);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(5.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 2.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 2.5);
    // Duration is 2s but at a 0.5x speed it takes 4s to end
    // When advancing 5.0s, there are still 1s remaining (spilled)
    REQUIRE(animationStateInstance->animationInstance()->spilledTime() == 1.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance spilledTime accounts for Nx speed with loop",
          "[animation]")
{

    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 2
    linearAnimation->duration(4);
    linearAnimation->fps(2);
    linearAnimation->speed(2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(5.5, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 1.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 11.0);
    // Duration is 2s but at a 2x speed it takes 1s to loop
    // When advancing 5.5s, there is still 0.5s remaining (spilled)
    REQUIRE(animationStateInstance->animationInstance()->spilledTime() == 0.5);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE(
    "AnimationStateInstance spilledTime accounts for 1/Nx speed with loop",
    "[animation]")
{
    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 2
    linearAnimation->duration(4);
    linearAnimation->fps(2);
    linearAnimation->speed(0.5);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(10.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 1.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 5.0);
    // Duration is 2s but at a 2x speed it takes 1s to loop
    // When advancing 5.5s, there is still 0.5s remaining (spilled)
    REQUIRE(animationStateInstance->animationInstance()->spilledTime() == 2.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE(
    "AnimationStateInstance spilledTime accounts for -Nx speed with oneShot",
    "[animation]")
{

    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 2
    linearAnimation->duration(4);
    linearAnimation->fps(2);
    linearAnimation->speed(-2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(3.0, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 0.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 6.0);
    // Duration is 2s but at a -2x speed it takes 1s to end
    // When advancing at negative speed, time starts at duration
    // so starting at end and taking 1s to complete
    // there are still 2s remaining (spilled)
    REQUIRE(animationStateInstance->animationInstance()->spilledTime() == 2.0);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}

TEST_CASE("AnimationStateInstance spilledTime accounts for -Nx speed with loop",
          "[animation]")
{

    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::StateMachine machine;
    rive::StateMachineInstance stateMachineInstance(&machine, abi.get());

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    // duration in seconds is 2
    linearAnimation->duration(4);
    linearAnimation->fps(2);
    linearAnimation->speed(-2);
    linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

    rive::AnimationState* animationState = new rive::AnimationState();
    animationState->animation(linearAnimation);

    rive::AnimationStateInstance* animationStateInstance =
        new rive::AnimationStateInstance(animationState, abi.get());

    // play from beginning.
    animationStateInstance->advance(5.5, &stateMachineInstance);

    REQUIRE(animationStateInstance->animationInstance()->time() == 1.0);
    REQUIRE(animationStateInstance->animationInstance()->totalTime() == 11.0);
    // Duration is 2s but at a -2x speed it takes 1s to end
    // When advancing at negative speed, time starts at duration
    // so starting at end and taking 1s to complete, it loops 5 times
    // there is still 0.5s remaining (spilled)
    REQUIRE(animationStateInstance->animationInstance()->spilledTime() == 0.5);

    delete animationStateInstance;
    delete animationState;
    delete linearAnimation;
}