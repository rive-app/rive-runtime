#include "catch.hpp"
#include "animation/loop.hpp"
#include "animation/linear_animation.hpp"
#include "animation/linear_animation_instance.hpp"
#include <cstdio>

TEST_CASE("LinearAnimationInstance oneShot", "[animation]")
{
	rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
	// duration in seconds is 5
	linearAnimation->duration(10);
	linearAnimation->fps(2);
	linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

	rive::LinearAnimationInstance* linearAnimationInstance =
	    new rive::LinearAnimationInstance(linearAnimation);

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

TEST_CASE("LinearAnimationInstance oneShot <-", "[animation]")
{
	rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
	// duration in seconds is 5
	linearAnimation->duration(10);
	linearAnimation->fps(2);
	linearAnimation->loopValue(static_cast<int>(rive::Loop::oneShot));

	rive::LinearAnimationInstance* linearAnimationInstance =
	    new rive::LinearAnimationInstance(linearAnimation);
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
	rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
	// duration in seconds is 5
	linearAnimation->duration(10);
	linearAnimation->fps(2);
	linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

	rive::LinearAnimationInstance* linearAnimationInstance =
	    new rive::LinearAnimationInstance(linearAnimation);

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
	rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
	// duration in seconds is 5
	linearAnimation->duration(10);
	linearAnimation->fps(2);
	linearAnimation->loopValue(static_cast<int>(rive::Loop::loop));

	rive::LinearAnimationInstance* linearAnimationInstance =
	    new rive::LinearAnimationInstance(linearAnimation);
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

TEST_CASE("LinearAnimationInstance pingpong ->", "[animation]")
{
	rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
	// duration in seconds is 5
	linearAnimation->duration(10);
	linearAnimation->fps(2);
	linearAnimation->loopValue(static_cast<int>(rive::Loop::pingPong));

	rive::LinearAnimationInstance* linearAnimationInstance =
	    new rive::LinearAnimationInstance(linearAnimation);

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

	delete linearAnimationInstance;
	delete linearAnimation;
}

TEST_CASE("LinearAnimationInstance pingpong <-", "[animation]")
{
	rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
	// duration in seconds is 5
	linearAnimation->duration(10);
	linearAnimation->fps(2);
	linearAnimation->loopValue(static_cast<int>(rive::Loop::pingPong));

	rive::LinearAnimationInstance* linearAnimationInstance =
	    new rive::LinearAnimationInstance(linearAnimation);
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