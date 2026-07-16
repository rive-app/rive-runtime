#include <rive/file.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/node.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

// The fixture (assets/data_bind_keyframes_test.riv) plays, via a state machine,
// one animation that keyframes a text run's text and a node's x. Those keyframe
// values are data bound to the default view model: the text run's start/end
// string keyframes bind to `keyfTextStart`/`keyfrTextEnd`, and the node's start
// x keyframe binds to `startX`. At time 0 the start keyframes apply, so the
// bound start values are what land on the targets; the node then tweens from
// the bound start toward its (authored) end.

TEST_CASE("Data binding keyframes", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    auto renderer = silver.makeRenderer();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.2f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.2f);
        artboard->draw(renderer.get());
    }

    auto keyfTextStartProp =
        vmi->propertyValue("keyfTextStart")->as<ViewModelInstanceString>();
    keyfTextStartProp->propertyValue("updated--text");

    auto colorStartProp =
        vmi->propertyValue("colorStart")->as<ViewModelInstanceColor>();
    auto yellowColor = (255 << 24) | (255 << 16) | (255 << 8);
    colorStartProp->propertyValue(yellowColor);

    auto startXProp =
        vmi->propertyValue("startX")->as<ViewModelInstanceNumber>();
    startXProp->propertyValue(100);

    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.2f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("data_bind_keyframes_test"));
}

// ---- helpers -------------------------------------------------------------

static TextValueRun* firstTextRun(Artboard* artboard)
{
    auto runs = artboard->find<TextValueRun>();
    return runs.empty() ? nullptr : runs[0];
}

static bool anyNodeHasX(Artboard* artboard, float x)
{
    for (auto* node : artboard->find<Node>())
    {
        if (node->x() == Approx(x))
        {
            return true;
        }
    }
    return false;
}

static void setStartText(rcp<ViewModelInstance> vmi, const std::string& text)
{
    vmi->propertyValue("keyfTextStart")
        ->as<ViewModelInstanceString>()
        ->propertyValue(text);
}

static void setStartX(rcp<ViewModelInstance> vmi, float x)
{
    vmi->propertyValue("startX")->as<ViewModelInstanceNumber>()->propertyValue(
        x);
}

// ---- behavioral tests ----------------------------------------------------

TEST_CASE("keyframe value binds resolve view-model values on the first frame",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto sm = artboard->stateMachineAt(0);
    REQUIRE(sm != nullptr);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(vmi != nullptr);

    // Distinctive sentinels set before binding, so the very first applied frame
    // must reflect them (holders are primed when the data bind is added).
    setStartText(vmi, "SENTINEL_START");
    setStartX(vmi, 424242.0f);

    sm->bindViewModelInstance(vmi);
    sm->advanceAndApply(0.0f);

    auto* run = firstTextRun(artboard.get());
    REQUIRE(run != nullptr);
    CHECK(run->text() == "SENTINEL_START");        // KeyFrameString bind
    CHECK(anyNodeHasX(artboard.get(), 424242.0f)); // KeyFrameDouble bind
}

TEST_CASE("keyframe value binds update when the source view-model changes",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto sm = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    setStartText(vmi, "first");
    setStartX(vmi, 10.0f);
    sm->bindViewModelInstance(vmi);
    sm->advanceAndApply(0.0f);

    auto* run = firstTextRun(artboard.get());
    REQUIRE(run != nullptr);
    REQUIRE(run->text() == "first");
    REQUIRE(anyNodeHasX(artboard.get(), 10.0f));

    // Change the sources; dt=0 keeps the playhead on the start keyframe, so the
    // exact new bound values must land after the next advance.
    setStartText(vmi, "second");
    setStartX(vmi, 987.0f);
    sm->advanceAndApply(0.0f);

    CHECK(run->text() == "second");
    CHECK(anyNodeHasX(artboard.get(), 987.0f));
}

TEST_CASE("keyframe interpolation reads the data-bound start value",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto sm = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    // A large, distinctive start value the authored keyframe would never
    // produce. The node tweens from this bound start toward its authored end,
    // so once it's into the tween its x must be strictly between the two,
    // proving the interpolation's from-endpoint is the bound value (not the
    // authored one).
    const float boundStart = 100000.0f;
    setStartX(vmi, boundStart);
    sm->bindViewModelInstance(vmi);

    sm->advanceAndApply(0.0f);
    REQUIRE(anyNodeHasX(artboard.get(), boundStart)); // start endpoint applied

    sm->advanceAndApply(0.5f); // well into the tween
    bool inTween = false;
    for (auto* node : artboard->find<Node>())
    {
        // Moved off the bound start but still dominated by it => interpolating
        // from the bound value.
        if (node->x() > 50000.0f && node->x() < boundStart)
        {
            inTween = true;
            break;
        }
    }
    CHECK(inTween);
}

TEST_CASE("standalone animation instance ignores keyframe value binds",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->animationCount() > 0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    setStartText(vmi, "SHOULD_NOT_BIND");
    setStartX(vmi, 424242.0f);
    // Bind the artboard's data context but drive playback via a *standalone*
    // LinearAnimationInstance (not a state machine). Keyframe value binds are
    // built only for state-machine-driven instances, so the authored keyframe
    // values must apply here instead of the bound ones.
    artboard->bindViewModelInstance(vmi);

    auto animation = artboard->animationAt(0);
    REQUIRE(animation != nullptr);
    animation->advanceAndApply(0.0f);

    auto* run = firstTextRun(artboard.get());
    REQUIRE(run != nullptr);
    CHECK(run->text() != "SHOULD_NOT_BIND");
    CHECK_FALSE(anyNodeHasX(artboard.get(), 424242.0f));
}
