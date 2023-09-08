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

TEST_CASE("AnimationStateInstance advances in step with animation speed 1", "[animation]")
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

TEST_CASE("AnimationStateInstance advances twice as fast when speed is doubled", "[animation]")
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

TEST_CASE("AnimationStateInstance advances half as fast when speed is halved", "[animation]")
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

TEST_CASE("AnimationStateInstance advances backwards when speed is negative", "[animation]")
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

TEST_CASE("AnimationStateInstance starts a positive animation at the beginning", "[animation]")
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

TEST_CASE("AnimationStateInstance starts a negative animation at the end", "[animation]")
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

TEST_CASE("AnimationStateInstance with negative speed starts a positive animation at the end",
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

TEST_CASE("AnimationStateInstance with negative speed starts a negative animation at the beginning",
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