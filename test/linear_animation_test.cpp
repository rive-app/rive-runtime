#include "rive/artboard.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
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

TEST_CASE("LinearAnimation reports when to keep going correctly", "[animation]")
{
    rive::NoOpFactory emptyFactory;
    // For each of these tests, we cons up a dummy artboard/instance
    // just to make the animations happy.
    rive::Artboard ab(&emptyFactory);
    auto abi = ab.instance();

    rive::LinearAnimation* linearAnimation = new rive::LinearAnimation();
    linearAnimation->duration(60);
    linearAnimation->fps(60);
    linearAnimation->speed(1);
    linearAnimation->enableWorkArea(true);
    linearAnimation->workStart(30);
    linearAnimation->workEnd(42);

    auto animationInstance = rive::LinearAnimationInstance(linearAnimation, abi.get());

    REQUIRE(animationInstance.advance(0.0f));
    REQUIRE(animationInstance.time() == 0.5f);
    REQUIRE(animationInstance.advance(0.1f));
    REQUIRE(animationInstance.time() == 0.6f);
    REQUIRE(!animationInstance.advance(0.2f));
    REQUIRE(animationInstance.time() == 0.7f);

    delete linearAnimation;
}

class TestReporter : public rive::KeyedCallbackReporter
{
private:
    std::vector<uint32_t> m_reportedObjects;

public:
    void reportKeyedCallback(uint32_t objectId, uint32_t propertyKey, float elapsedSeconds) override
    {
        m_reportedObjects.push_back(objectId);
    }

    size_t count() { return m_reportedObjects.size(); }
};

TEST_CASE("Looping timeline events load correctly and report", "[events]")
{
    auto file = ReadRiveFile("../../test/assets/looping_timeline_events.riv");

    auto artboard = file->artboard()->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() == 1);

    auto animationInstance = artboard->animationAt(0);
    REQUIRE(animationInstance != nullptr);

    TestReporter reporter;
    // Report (at frame 0) fires from 0-0.1f
    animationInstance->advance(0.1f, &reporter);
    REQUIRE(animationInstance->time() == 0.1f);
    REQUIRE(reporter.count() == 1);

    // Report (at frame 25) fires
    animationInstance->advance(0.32f, &reporter);
    REQUIRE(animationInstance->time() == 0.42f);
    REQUIRE(reporter.count() == 2);

    // No report from .42 to .72 seconds.
    animationInstance->advance(0.3f, &reporter);
    REQUIRE(animationInstance->time() == 0.72f);
    REQUIRE(reporter.count() == 2);

    // Report at 1s fires from .72 to 1.0 seconds.
    animationInstance->advance(0.28f, &reporter);
    REQUIRE(animationInstance->time() == 0.0f);
    REQUIRE(reporter.count() == 3);

    // All 3 events fire (and first one again too).
    animationInstance->advance(1.01f, &reporter);
    REQUIRE(animationInstance->time() == Approx(0.01f));
    REQUIRE(reporter.count() == 7);
}