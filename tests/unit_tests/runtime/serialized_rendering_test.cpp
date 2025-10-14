#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/math/random.hpp"
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

TEST_CASE("hide silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/hide_test.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(1, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("hide_test"));
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

TEST_CASE("lock icon listener silver", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/lock_icon_demo.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Click in the middle of the state machine.
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() / 2.0f, artboard->height() / 2.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    artboard->draw(renderer.get());

    silver.addFrame();

    // Do it again to lock the icon.
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() / 2.0f, artboard->height() / 2.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("lock_icon_demo"));
}

TEST_CASE("validate text run listener works", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/text_listener_simpler.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Click in the middle of the state machine.
    stateMachine->pointerDown(
        rive::Vec2D(artboard->width() * 0.8, artboard->height() / 2.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("text_listener_simpler"));
}

TEST_CASE("validate text with modifiers and dashes render correctly",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/text_stroke_test.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    // Draw first frame.
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("text_stroke_test"));
}

TEST_CASE("superbowl data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/superbowl.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(10.0f / 1.0f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(1.0f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("superbowl"));
}

TEST_CASE("data viz demo data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_viz_demo.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto vmiItem1 = vmi->propertyValue("item1");
    auto vmiItem1ViewModelInstance = vmiItem1->as<ViewModelInstanceViewModel>()
                                         ->referenceViewModelInstance()
                                         .get();
    if (vmiItem1ViewModelInstance != nullptr)
    {
        auto valueProp = vmiItem1ViewModelInstance->propertyValue("value");
        if (valueProp != nullptr)
        {
            valueProp->as<ViewModelInstanceNumber>()->propertyValue(20);
        }
    }

    int frames = (int)(0.8f / 0.064f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.064f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("data_viz_demo"));
}

TEST_CASE("bank card data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/bankcard.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.1f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("bankcard"));
}

TEST_CASE("ai assistant data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/ai_assitant.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto leftNumber = vmi->propertyValue("left");
    auto bottomNumber = vmi->propertyValue("bottom");
    auto topNumber = vmi->propertyValue("top");
    auto rightNumber = vmi->propertyValue("right");

    int frames = (int)(1.0f / 0.33f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        leftNumber->as<ViewModelInstanceNumber>()->propertyValue(i * 10);
        bottomNumber->as<ViewModelInstanceNumber>()->propertyValue(i * 5);
        topNumber->as<ViewModelInstanceNumber>()->propertyValue(i * 3);
        rightNumber->as<ViewModelInstanceNumber>()->propertyValue(i * 2);
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("ai_assitant"));
}

TEST_CASE("echo show demo data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/echo_show_demo.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("echo_show_demo"));
}

TEST_CASE("rewards demo data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/rewards_demo.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto buttonProp = vmi->propertyValue("Button");
    if (buttonProp != nullptr)
    {
        auto buttonVm = buttonProp->as<ViewModelInstanceViewModel>()
                            ->referenceViewModelInstance()
                            .get();
        if (buttonVm != nullptr)
        {
            auto trigger = buttonVm->propertyValue("Pressed");
            trigger->as<ViewModelInstanceTrigger>()->trigger();
        }
    }

    int frames = (int)(5.0f / 0.25f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("rewards_demo"));
}

TEST_CASE("spotify kids demo data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/spotify_kids_demo.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("spotify_kids_demo"));
}

TEST_CASE("spotify kids app icon data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/spotify_kids_app_icon.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("spotify_kids_app_icon"));
}

TEST_CASE("hunter x demo data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/hunter_x_demo.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("hunter_x_demo"));
}

TEST_CASE("health tracker data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/db_health_tracker.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("db_health_tracker"));
}

TEST_CASE("car widgets data binding", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/car_widgets_v01.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto vmiItem1 = vmi->propertyValue("COMPASS");
    auto vmiItem1ViewModelInstance = vmiItem1->as<ViewModelInstanceViewModel>()
                                         ->referenceViewModelInstance()
                                         .get();
    if (vmiItem1ViewModelInstance != nullptr)
    {
        auto valueProp = vmiItem1ViewModelInstance->propertyValue("Rotation");
        if (valueProp != nullptr)
        {
            valueProp->as<ViewModelInstanceNumber>()->propertyValue(20);
        }
    }

    auto vmiItem2 = vmi->propertyValue("TIRE PSI");
    auto vmiItem2ViewModelInstance = vmiItem2->as<ViewModelInstanceViewModel>()
                                         ->referenceViewModelInstance()
                                         .get();
    if (vmiItem2ViewModelInstance != nullptr)
    {
        auto valueProp = vmiItem2ViewModelInstance->propertyValue("FL Tyre");
        if (valueProp != nullptr)
        {
            valueProp->as<ViewModelInstanceNumber>()->propertyValue(10);
        }
    }

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("car_widgets_v01"));
}

TEST_CASE("Vertical align on text with ellipsis", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/vertical_align_ellipsis.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("vertical_align_ellipsis"));
}

TEST_CASE("Event triggers another event", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/event_trigger_event.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Click in a square that fires a trigger. This trigger will cause a
    // transition that fires an event. There is a listener on that event that
    // fires a second event.
    stateMachine->pointerDown(rive::Vec2D(475, 25));
    stateMachine->pointerUp(rive::Vec2D(475, 25));
    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("event_trigger_event"));
}

TEST_CASE("Collapsed data binds from layouts with display hidden", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/collapse_data_binds.riv", &silver);

    auto artboard = file->artboardNamed("test-1");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("collapse_data_binds-test_1"));
}

TEST_CASE("Collapsed data binds from property groups in solos", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/collapse_data_binds.riv", &silver);

    auto artboard = file->artboardNamed("test-2");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("collapse_data_binds-test_2"));
}

TEST_CASE("Collapsed data bound layout styles still update", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/collapse_data_binds.riv", &silver);

    auto artboard = file->artboardNamed("test-3");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto display1 =
        vmi->propertyValue("display_1")->as<ViewModelInstanceEnum>();

    auto display2 =
        vmi->propertyValue("display_2")->as<ViewModelInstanceEnum>();

    stateMachine->advanceAndApply(0.0f);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    display2->value(1); // Hide inner layout
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    display1->value(1); // Hide outer layout
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    display2->value(0); // Show inner layout (nothing changes)
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    display1->value(0); // Show outer layout (nothing changes)
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("collapse_data_binds-test_3"));
}

TEST_CASE("Reset randomization on value change", "[silver]")
{
    RandomProvider::clearRandoms();
    REQUIRE(RandomProvider::totalCalls() == 0);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/formula_random.riv", &silver);

    auto artboard = file->artboardNamed("source_change");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto numProp = vmi->propertyValue("n1")->as<ViewModelInstanceNumber>();

    RandomProvider::addRandomValue(0.0f);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    REQUIRE(RandomProvider::totalCalls() == 1);

    silver.addFrame();

    RandomProvider::addRandomValue(1.0f);
    numProp->propertyValue(500);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // Random generation hasn't been called again
    REQUIRE(RandomProvider::totalCalls() == 2);
    RandomProvider::clearRandoms();
    CHECK(silver.matches("formula_random-source_change"));
}

TEST_CASE("Reset randomization only once", "[silver]")
{
    RandomProvider::clearRandoms();
    REQUIRE(RandomProvider::totalCalls() == 0);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/formula_random.riv", &silver);

    auto artboard = file->artboardNamed("once");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto numProp = vmi->propertyValue("n1")->as<ViewModelInstanceNumber>();

    RandomProvider::addRandomValue(0.0f);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    REQUIRE(RandomProvider::totalCalls() == 1);

    silver.addFrame();

    RandomProvider::addRandomValue(1.0f);
    numProp->propertyValue(500);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // Random generation has been called again
    REQUIRE(RandomProvider::totalCalls() == 1);
    RandomProvider::clearRandoms();
    CHECK(silver.matches("formula_random-once"));
}

TEST_CASE("Reset randomization on every change", "[silver]")
{
    RandomProvider::clearRandoms();
    REQUIRE(RandomProvider::totalCalls() == 0);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/formula_random.riv", &silver);

    auto artboard = file->artboardNamed("always");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto numProp = vmi->propertyValue("n1")->as<ViewModelInstanceNumber>();

    RandomProvider::addRandomValue(0.0f);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    REQUIRE(RandomProvider::totalCalls() == 1);

    silver.addFrame();

    numProp->propertyValue(500);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // Random generation hasn't been called on every advance
    REQUIRE(RandomProvider::totalCalls() == 64);
    RandomProvider::clearRandoms();
    CHECK(silver.matches("formula_random-always"));
}

TEST_CASE("Target to source with different data types on source and target",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/saturation.riv", &silver);

    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("saturation"));
}

TEST_CASE("interactive and non interactive scrolling", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/interactive_scrolling.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto boolProp =
        vmi->propertyValue("isInteractive")->as<ViewModelInstanceBoolean>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    double yPos = artboard->height() - 20;

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(artboard->width() / 2.0f, yPos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (yPos > 120)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(artboard->width() / 2.0f, yPos));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        yPos -= 20;
    }
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(artboard->width() / 2.0f, yPos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change to interactive

    boolProp->propertyValue(true);
    stateMachine->advanceAndApply(0.1f);

    yPos = artboard->height() - 20;

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(artboard->width() / 2.0f, yPos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    while (yPos > 120)
    {

        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(artboard->width() / 2.0f, yPos));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        yPos -= 20;
    }
    silver.addFrame();
    stateMachine->pointerUp(rive::Vec2D(artboard->width() / 2.0f, yPos));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("interactive_scrolling"));
}

TEST_CASE("Interpolator returns advance status as true until it settles",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/interpolate_to_end.riv", &silver);

    auto artboard = file->artboardNamed("child");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto numProp = vmi->propertyValue("num")->as<ViewModelInstanceNumber>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    numProp->propertyValue(1000);
    stateMachine->advanceAndApply(0.001f);

    auto shouldAdvance = true;
    while (shouldAdvance)
    {
        silver.addFrame();
        shouldAdvance = stateMachine->advanceAndApply(0.25f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("interpolate_to_end"));
}