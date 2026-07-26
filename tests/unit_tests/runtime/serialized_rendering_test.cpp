#include "rive/file.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/math/random.hpp"
// #include "rive/input/gamepad_batch.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>
#include "rive/profiler/profiler_macros.h"
#include <filesystem>
#include <fstream>
#include <iostream>

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
    File::deterministicMode = true;
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
    File::deterministicMode = false;
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
    File::deterministicMode = true;
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
    File::deterministicMode = false;
}

TEST_CASE("Reset randomization only once", "[silver]")
{
    File::deterministicMode = true;
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
    File::deterministicMode = false;
}

TEST_CASE("Reset randomization on every change", "[silver]")
{
    File::deterministicMode = true;
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
    File::deterministicMode = false;
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

TEST_CASE("View models keep reference to their dependents and parents",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/replace_vm_instance.riv", &silver);

    auto artboardMain1 = file->artboardNamed("main-1");
    auto artboardMain2 = file->artboardNamed("main-2");
    auto artboardCommon = file->artboardNamed("common");
    REQUIRE(artboardMain1 != nullptr);
    REQUIRE(artboardMain2 != nullptr);
    REQUIRE(artboardCommon != nullptr);

    silver.frameSize(artboardMain1->width(), artboardMain1->height());

    std::vector<StateMachineInstance*> stateMachines;

    auto stateMachineMain1 = artboardMain1->stateMachineAt(0);
    auto stateMachineMain2 = artboardMain2->stateMachineAt(0);
    auto stateMachineCommon = artboardCommon->stateMachineAt(0);
    stateMachines.push_back(stateMachineMain1.get());
    stateMachines.push_back(stateMachineMain2.get());
    stateMachines.push_back(stateMachineCommon.get());

    // Each view model instance has its own independent tree
    auto vmiMain1 =
        file->createViewModelInstance(artboardMain1.get()->viewModelId(), 0);
    auto vmiMain2 =
        file->createViewModelInstance(artboardMain2.get()->viewModelId(), 0);
    auto vmiCommon =
        file->createViewModelInstance(artboardCommon.get()->viewModelId(), 0);

    stateMachineMain1->bindViewModelInstance(vmiMain1);
    stateMachineMain1->advanceAndApply(0.1f);
    stateMachineMain2->bindViewModelInstance(vmiMain2);
    stateMachineMain2->advanceAndApply(0.1f);
    stateMachineCommon->bindViewModelInstance(vmiCommon);
    stateMachineCommon->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();

    artboardMain1->draw(renderer.get());
    silver.addFrame();
    artboardMain2->draw(renderer.get());
    silver.addFrame();
    artboardCommon->draw(renderer.get());

    for (auto& sm : stateMachines)
    {
        silver.addFrame();
        sm->advanceAndApply(0.1f);
        sm->artboard()->draw(renderer.get());
    }

    // Changing the text value of one instance only affects that instance
    auto vmCommon1Prop = vmiMain1->propertyValue("common-1")
                             ->as<ViewModelInstanceViewModel>()
                             ->referenceViewModelInstance()
                             .get();
    auto vmChildProp = vmCommon1Prop->propertyValue("ch")
                           ->as<ViewModelInstanceViewModel>()
                           ->referenceViewModelInstance();
    auto stringProp =
        vmChildProp.get()->propertyValue("str")->as<ViewModelInstanceString>();
    stringProp->propertyValue("update-1");

    for (auto& sm : stateMachines)
    {
        silver.addFrame();
        sm->advanceAndApply(0.1f);
        sm->artboard()->draw(renderer.get());
    }

    // Replace the nested child view model of main2 with the same instance of
    // main1 Instance 2 should update
    auto vmCommonMain2Prop = vmiMain2->propertyValue("common-2")
                                 ->as<ViewModelInstanceViewModel>()
                                 ->referenceViewModelInstance()
                                 .get();

    vmCommonMain2Prop->replaceViewModelByName("ch", vmChildProp);

    for (auto& sm : stateMachines)
    {
        silver.addFrame();
        sm->advanceAndApply(0.1f);
        sm->artboard()->draw(renderer.get());
    }

    // Updating the value, changes it on both artboard

    stringProp->propertyValue("update-2");

    for (auto& sm : stateMachines)
    {
        silver.addFrame();
        sm->advanceAndApply(0.1f);
        sm->artboard()->draw(renderer.get());
    }

    // Replace the child view model of common with the same instance of
    // main1
    // common should update now too

    auto currentChild =
        vmiCommon->propertyValue("ch")->as<ViewModelInstanceViewModel>();
    auto referenceViewModel = currentChild->referenceViewModelInstance();
    REQUIRE(referenceViewModel->parents().size() == 1);
    vmiCommon->replaceViewModelByName("ch", vmChildProp);
    REQUIRE(referenceViewModel->parents().size() == 0);

    REQUIRE(vmiMain1->dependents().size() == 1);
    REQUIRE(vmiMain2->dependents().size() == 1);
    stateMachineMain2->bindViewModelInstance(vmiMain1);
    REQUIRE(vmiMain1->dependents().size() == 2);
    REQUIRE(vmiMain2->dependents().size() == 0);

    for (auto& sm : stateMachines)
    {
        silver.addFrame();
        sm->advanceAndApply(0.1f);
        sm->artboard()->draw(renderer.get());
    }

    CHECK(silver.matches("replace_vm_instance"));
}

TEST_CASE("Replace view model instance in list", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/replace_vm_instance.riv", &silver);

    auto artboard = file->artboardNamed("main-list");

    silver.frameSize(artboard->width(), artboard->height());

    auto renderer = silver.makeRenderer();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Replace an instance of a list and expect the binding to be automatically
    // called
    silver.addFrame();

    // First create a instance
    auto grandChildInstance =
        file->createViewModelInstance("main-list-grandchild");
    auto labelProp = grandChildInstance->propertyValue("label")
                         ->as<ViewModelInstanceString>();
    labelProp->propertyValue("modified");

    // Second retrieve the item from the list that will be replaced
    auto lis = vmi->propertyValue("lis")->as<ViewModelInstanceList>();
    {
        auto listItem = lis->item(1);
        auto listItemViewModelInstance = listItem->viewModelInstance();

        // Third replace the property "grandchild" with the new instance created
        listItemViewModelInstance->replaceViewModelByName("grandchild",
                                                          grandChildInstance);

        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Create a new child instance and append it to the list, then replace its
    // child
    {
        silver.addFrame();

        // First create instance to be added and set its label value

        auto childInstance = file->createViewModelInstance("main-list-child");
        auto childGrandChild = childInstance->propertyValue("grandchild")
                                   ->as<ViewModelInstanceViewModel>()
                                   ->referenceViewModelInstance()
                                   .get();
        auto childGrandChildLabel = childGrandChild->propertyValue("label")
                                        ->as<ViewModelInstanceString>();
        childGrandChildLabel->propertyValue("new label");

        // Second append it to the list
        auto listItem = make_rcp<ViewModelInstanceListItem>();
        listItem->viewModelInstance(childInstance);
        lis->addItem(listItem);

        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());

        // Replace the view model property and expect the instance to be updated

        silver.addFrame();
        auto grandChildInstance =
            file->createViewModelInstance("main-list-grandchild");
        auto labelProp = grandChildInstance->propertyValue("label")
                             ->as<ViewModelInstanceString>();
        labelProp->propertyValue("modified 2");
        childInstance->replaceViewModelByName("grandchild", grandChildInstance);
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("replace_vm_instance-list"));
}

TEST_CASE("Replace view model instance in nested list", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/replace_vm_instance.riv", &silver);

    auto artboard = file->artboardNamed("main-double-nest");

    silver.frameSize(artboard->width(), artboard->height());

    auto renderer = silver.makeRenderer();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Replace an instance of a list and expect the binding to be automatically
    // called
    silver.addFrame();

    // First create a instance
    auto grandChildInstance =
        file->createViewModelInstance("main-list-grandchild");
    auto labelProp = grandChildInstance->propertyValue("label")
                         ->as<ViewModelInstanceString>();
    labelProp->propertyValue("modified");

    auto mainList = vmi->propertyValue("lis")->as<ViewModelInstanceList>();
    auto mainListChild = mainList->item(0);
    auto mainListChildInstance = mainListChild->viewModelInstance();

    // Second retrieve the item from the list that will be replaced
    auto lis = mainListChildInstance->propertyValue("lis")
                   ->as<ViewModelInstanceList>();
    {
        auto listItem = lis->item(1);
        auto listItemViewModelInstance = listItem->viewModelInstance();

        // Third replace the property "grandchild" with the new instance created
        listItemViewModelInstance->replaceViewModelByName("grandchild",
                                                          grandChildInstance);

        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    // Create a new child instance and append it to the list, then replace its
    // child
    {
        silver.addFrame();

        // First create instance to be added and set its label value

        auto childInstance = file->createViewModelInstance("main-list-child");
        auto childGrandChild = childInstance->propertyValue("grandchild")
                                   ->as<ViewModelInstanceViewModel>()
                                   ->referenceViewModelInstance()
                                   .get();
        auto childGrandChildLabel = childGrandChild->propertyValue("label")
                                        ->as<ViewModelInstanceString>();
        childGrandChildLabel->propertyValue("new label");

        // Second append it to the list
        auto listItem = make_rcp<ViewModelInstanceListItem>();
        listItem->viewModelInstance(childInstance);
        lis->addItem(listItem);

        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());

        // Replace the view model property and expect the instance to be updated

        silver.addFrame();
        auto grandChildInstance =
            file->createViewModelInstance("main-list-grandchild");
        auto labelProp = grandChildInstance->propertyValue("label")
                             ->as<ViewModelInstanceString>();
        labelProp->propertyValue("modified 2");
        childInstance->replaceViewModelByName("grandchild", grandChildInstance);
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("replace_vm_instance-double-nest"));
}

TEST_CASE("Pointer drag event", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/drag_event.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    // Clicking on a square without moving will trigger the click on the nested
    // artboard
    stateMachine->pointerDown(rive::Vec2D(250, 250));
    stateMachine->pointerUp(rive::Vec2D(250, 250));
    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    silver.addFrame();

    auto coord = 250.0f;
    stateMachine->pointerDown(rive::Vec2D(coord, coord));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Drag gesture works and click is cancelled
    while (coord > 50)
    {
        silver.addFrame();
        stateMachine->pointerMove(rive::Vec2D(coord, coord));
        stateMachine->advanceAndApply(0.1f);
        artboard->draw(renderer.get());
        coord -= 10;
    }
    stateMachine->pointerUp(rive::Vec2D(coord, coord));

    silver.addFrame();
    // Clicking again on a square without moving will trigger the click on the
    // nested artboard
    stateMachine->pointerDown(rive::Vec2D(coord, coord));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->pointerUp(rive::Vec2D(coord, coord));
    stateMachine->advanceAndApply(0.1f);

    artboard->draw(renderer.get());

    CHECK(silver.matches("drag_event"));
}

TEST_CASE("Recursive data binding artboards are skipped", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/recursive_data_bind.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(249.0f, 430.0f));
    stateMachine->pointerUp(rive::Vec2D(249.0f, 430.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(391.0f, 430.0f));
    stateMachine->pointerUp(rive::Vec2D(391.0f, 430.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();

    // This action will bind to a recursive artboard and should be skipped
    stateMachine->pointerDown(rive::Vec2D(107.0f, 430.0f));
    stateMachine->pointerUp(rive::Vec2D(107.0f, 430.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(249.0f, 430.0f));
    stateMachine->pointerUp(rive::Vec2D(249.0f, 430.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(391.0f, 430.0f));
    stateMachine->pointerUp(rive::Vec2D(391.0f, 430.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("recursive_data_bind"));
}

TEST_CASE("Collapsable data binds get added when object is uncollapsed",
          "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/collapsable_data_binding.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);

    auto soloIndexProp =
        vmi->propertyValue("soloIndex")->as<ViewModelInstanceNumber>();
    auto colorProp = vmi->propertyValue("col")->as<ViewModelInstanceColor>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    auto redColor = (255 << 24) | (255 << 16);
    // Setting the red color should update both data binds although one is
    // soloed
    colorProp->propertyValue(redColor);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();

    soloIndexProp->propertyValue(1);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    auto greenColor = (255 << 24) | (255 << 8);
    colorProp->propertyValue(greenColor);
    soloIndexProp->propertyValue(0);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    soloIndexProp->propertyValue(1);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("collapsable_data_binding"));
}

// Note to this test for future reference: This test is passing because when an
// artboard list is initialized, it populates its nested artboards only after
// option C has advanced a second time. This causes tryChangeState to run before
// advancing, and that allows the state to be available for the first advance to
// mix its blend values. That's not the case for nested artboard, so if any of
// these premises changes in the future, this test would catch the change.
TEST_CASE(
    "Virtualized list with blended animations as initial state correctly render their mixed values",
    "[silver]")
{
    RandomProvider::clearRandoms();
    REQUIRE(RandomProvider::totalCalls() == 0);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/virtualize_blendmode.riv", &silver);

    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.016f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(4.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("virtualize_blendmode"));
}

TEST_CASE("Advance two consecutive blend modes and apply inputs", "[silver]")
{
    RandomProvider::clearRandoms();
    REQUIRE(RandomProvider::totalCalls() == 0);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/advance_blend_mode.riv", &silver);

    auto artboard = file->artboardNamed("main-inputs");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("advance_blend_mode-inputs"));
}

TEST_CASE("Advance two consecutive blend modes and apply view model",
          "[silver]")
{
    RandomProvider::clearRandoms();
    REQUIRE(RandomProvider::totalCalls() == 0);
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/advance_blend_mode.riv", &silver);

    auto artboard = file->artboardNamed("main-vms");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("advance_blend_mode-vms"));
}

TEST_CASE("Test State machine transition conditions based on artboards",
          "[silver]")
{
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/transition_artboard_condition_test.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("transition_artboard_condition_test"));
}

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file =
//         ReadRiveFile("assets/text_input_with_focus_listener.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = file->createViewModelInstance(artboard.get());
//     auto focusManager = artboard->focusManager();

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.0f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     stateMachine->pointerDown(Vec2D(50, 50));
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     printf("===> KEY DOWN\n");
//     focusManager->keyInput(rive::Key::d, rive::KeyModifiers::none, true,
//     false); stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     focusManager->keyInput(rive::Key::d, rive::KeyModifiers::none, false,
//     false); stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     // silver.addFrame();

//     CHECK(silver.matches("text_input_with_focus_listener"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     printf("=====================================> FILE 1\n");
//     auto file = ReadRiveFile("assets/dlab_reduced.riv", &silver);
//     printf("=====================================> FILE 2\n");
//     auto file2 =
//         ReadRiveFile("assets/avatar_reduced.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createViewModelInstance(artboard.get());
//     auto renderer = silver.makeRenderer();

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.0f);

//     auto avatarArtboard = file2->bindableArtboardNamed("avatar_artboard");
//     auto mainVM = file2->viewModel("main_vm");
//     auto mainVMInstance = mainVM->createInstance();
//     printf("====>> REPLACE\n");
//     vmi->replaceViewModelByName("nested_vm_1", mainVMInstance);

//     auto abProp = vmi->propertyValue("avatar_1_artboardProperty")
//                       ->as<ViewModelInstanceArtboard>();
//     abProp->asset(avatarArtboard);

//     auto newProp = vmi->propertyValue("nested_vm_1")
//                        ->as<ViewModelInstanceViewModel>()
//                        ->referenceViewModelInstance()
//                        ->propertyValue("circle_crop_db_bool")
//                        ->as<ViewModelInstanceBoolean>();
//     // newProp->propertyValue(true);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     // silver.addFrame();

//     CHECK(silver.matches("dlab_gifthub_v20_001"));
// // // }
// TEST_CASE("XXXXX", "[silver]")
// {
// auto file = ReadRiveFile("assets/flashcards_v15_04.riv");

// auto artboard = file->artboardDefault();
// REQUIRE(artboard != nullptr);

// auto stateMachine = artboard->stateMachineAt(0);

// auto vmi = file->createViewModelInstance(artboard.get());
// auto textKey = vmi->propertyValue("textKey")->as<ViewModelInstanceNumber>();
// auto inputText =
//     vmi->propertyValue("inputText")->as<ViewModelInstanceString>();
// CHECK(textKey->propertyValue() == 0);
// CHECK(inputText->propertyValue() == "");
// stateMachine->bindViewModelInstance(vmi);
// stateMachine->advanceAndApply(0.0f);

// auto focusManager = artboard->focusManager();
// // focusManager->focusNext();
// stateMachine->advanceAndApply(0.016f);

// focusManager->keyInput(Key::a, KeyModifiers::none, true, false);
// stateMachine->advanceAndApply(0.016f);
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/flashcards_v15_04.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = file->createViewModelInstance(artboard.get());
//     {

//         auto card_01 =
//             vmi->propertyValue("card_01")->as<ViewModelInstanceViewModel>();
//         auto card_01_ref = card_01->referenceViewModelInstance();
//         auto card_01_full =
//             card_01_ref->propertyValue("cjk_full_card_single_vm")
//                 ->as<ViewModelInstanceViewModel>();
//         auto card_01_full_ref = card_01_full->referenceViewModelInstance();
//         printf("card_01_full_ref: %p\n", card_01_full_ref.get());
//         auto cjk_list_db_property =
//             card_01_full_ref->propertyValue("cjk_list_db_property")
//                 ->as<ViewModelInstanceList>();

//         auto cjk_single_vm = file->viewModel("cjk_single_vm");
//         auto cjk_card_00 = cjk_single_vm->createInstance();
//         // Change symbol_str , romanized_tl_str, and furigana_tl_str on this
//         // instance
//         cjk_card_00->propertyValue("symbol_str")
//             ->as<ViewModelInstanceString>()
//             ->propertyValue("aaa");
//         cjk_card_00->propertyValue("romanized_tl_str")
//             ->as<ViewModelInstanceString>()
//             ->propertyValue("bbb");
//         cjk_card_00->propertyValue("furigana_tl_str")
//             ->as<ViewModelInstanceString>()
//             ->propertyValue("ccc");
//         auto listItem = make_rcp<ViewModelInstanceListItem>();
//         listItem->viewModelInstance(cjk_card_00);
//         cjk_list_db_property->addItem(listItem);
//         printf("listItems:size::b %zu at %p\n",
//                cjk_list_db_property->listItems().size(),
//                cjk_list_db_property);
//         auto firstItem = cjk_list_db_property->listItems()[0];
//         auto firstItemViewModel = firstItem->viewModelInstance();
//         auto symbol_str = firstItemViewModel->propertyValue("symbol_str")
//                               ->as<ViewModelInstanceString>();
//         printf("symbol_str: %s\n", symbol_str->propertyValue().c_str());
//     }
//     auto focusManager = artboard->focusManager();

//     stateMachine->bindViewModelInstance(vmi);

//     stateMachine->advanceAndApply(0.0f);
//     auto renderer = silver.makeRenderer();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Make a new view model instance from cjk_single_vm ; cjk_card_00
//     auto cjk_single_vm = file->viewModel("cjk_single_vm");
//     auto cjk_card_00 = cjk_single_vm->createInstance();
//     // Change symbol_str , romanized_tl_str, and furigana_tl_str on this
//     // instance
//     cjk_card_00->propertyValue("symbol_str")
//         ->as<ViewModelInstanceString>()
//         ->propertyValue("eee");
//     cjk_card_00->propertyValue("romanized_tl_str")
//         ->as<ViewModelInstanceString>()
//         ->propertyValue("fff");
//     cjk_card_00->propertyValue("furigana_tl_str")
//         ->as<ViewModelInstanceString>()
//         ->propertyValue("ggg");
//     // Make a new view model instance from cjk_full_vm ; card_00_cjk_full
//     auto cjk_full_vm = file->viewModel("cjk_full_vm");
//     auto card_00_cjk_full = cjk_full_vm->createInstance();
//     // Push cjk_card_00 in cjk_list_db_property that's inside
//     card_00_cjk_full auto cjk_list_db_property =
//         card_00_cjk_full->propertyValue("cjk_list_db_property")
//             ->as<ViewModelInstanceList>();
//     auto listItem = make_rcp<ViewModelInstanceListItem>();
//     listItem->viewModelInstance(cjk_card_00);
//     cjk_list_db_property->addItem(listItem);
//     // Make a new view model instance from card_single_vm, card_00_instance
//     auto card_single_vm = file->viewModel("card_single_vm");
//     auto card_00_instance = card_single_vm->createInstance();
//     // In card_00_instance, set the view model instance at path
//     // cjk_full_card_single_vm to card_00_cjk_full
//     card_00_instance->replaceViewModelByName("cjk_full_card_single_vm",
//                                              card_00_cjk_full);
//     // Inside the main view model instance, set the view model instance at
//     path
//     // card_01 to card_00_instance
//     vmi->replaceViewModelByName("card_01", card_00_instance);

//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     CHECK(silver.matches("flashcards_v15_04"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file =
//         ReadRiveFile("assets/rebind_with_nested_viewmodel.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     auto focusManager = artboard->focusManager();

//     stateMachine->bindViewModelInstance(vmi);

//     stateMachine->advanceAndApply(0.0f);
//     auto renderer = silver.makeRenderer();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     {
//         auto second =
//             vmi->propertyValue("sec")->as<ViewModelInstanceViewModel>();
//         auto secInstance = second->referenceViewModelInstance();
//         auto third =
//             secInstance->propertyValue("thi")->as<ViewModelInstanceViewModel>();
//         auto thirdInstance = third->referenceViewModelInstance();
//         thirdInstance->propertyValue("label")
//             ->as<ViewModelInstanceString>()
//             ->propertyValue("updated");
//     }
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Create a new view model with a new child view modoel
//     auto thirdVM = file->viewModel("Third");
//     auto thirdInstance = thirdVM->createInstance();
//     thirdInstance->propertyValue("label")
//         ->as<ViewModelInstanceString>()
//         ->propertyValue("new updated");
//     auto secondVM = file->viewModel("Second");
//     auto secondInstance = secondVM->createInstance();
//     secondInstance->replaceViewModelByName("thi", thirdInstance);
//     vmi->replaceViewModelByName("sec", secondInstance);

//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("rebind_with_nested_viewmodel"));
// }

// TEST_CASE("XXXXX", "[data binding]")
// {
//     auto file = ReadRiveFile("assets/blinko.riv");
//     REQUIRE(file != nullptr);

//     constexpr int NUM_CYCLES = 1;
//     constexpr int FRAMES_PER_CYCLE = 2;

//     // Warm up — first cycle allocates caches, font data, etc.
//     // {
//     auto artboard = file->artboardDefault()->instance();
//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     auto machine = artboard->defaultStateMachine();
//     machine->bindViewModelInstance(vmi);
//     // }
//     lua_gc(file->scriptingState(), LUA_GCCOLLECT, 0);

//     // size_t baselineHeap = getHeapUsage();

//     // for (int i = 0; i < NUM_CYCLES; i++)
//     // {
//     //     {
//     //         auto artboard = file->artboardDefault()->instance();
//     //         auto vmi =
//     file->createDefaultViewModelInstance(artboard.get());
//     //         auto machine = artboard->defaultStateMachine();
//     //         machine->bindViewModelInstance(vmi);
//     //         machine->advanceAndApply(0.0f);
//     //         for (int f = 0; f < FRAMES_PER_CYCLE; f++)
//     //             machine->advanceAndApply(0.016f);
//     //         // artboard, machine, vmi all go out of scope here
//     //     }

//     //     // size_t heap = getHeapUsage();
//     //     // printf("Cycle %2d: growth_from_baseline=%.1f MB\n",
//     //     //        i, (heap - baselineHeap) / (1024.0 * 1024.0));
//     // }

//     // size_t finalHeap = getHeapUsage();
//     // double totalGrowth = (double)(finalHeap - baselineHeap) / (1024.0 *
//     // 1024.0); printf("Total growth: %.1f MB (%.2f MB/cycle)\n",
//     // totalGrowth,
//     // totalGrowth / NUM_CYCLES);

//     // After all objects are destroyed, heap should return near baseline
//     // REQUIRE(totalGrowth < 5.0);  // Currently fails: ~63 MB growth
// // }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file =
//         ReadRiveFile("assets/scripted_data_converter_bound_input.riv",
//         &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = viewModelId == -1
//                    ? file->createViewModelInstance(artboard.get())
//                    : file->createViewModelInstance(viewModelId, 0);

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     silver.addFrame();

//     stateMachine->advanceAndApply(0.1f);

//     artboard->draw(renderer.get());

//     int frames = (int)(1.0f / 0.016f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.016f);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("scripted_data_converter_bound_input"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file =
//         ReadRiveFile("assets/controlling_node_read_data_with_scripting.riv",
//                      &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = viewModelId == -1
//                    ? file->createViewModelInstance(artboard.get())
//                    : file->createViewModelInstance(viewModelId, 0);

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     silver.addFrame();

//     stateMachine->pointerDown(Vec2D(50.0f, 50.0f));
//     stateMachine->pointerUp(Vec2D(50.0f, 50.0f));

//     // int frames = (int)(1.0f / 0.016f);
//     // for (int i = 0; i < frames; i++)
//     // {
//     //     silver.addFrame();
//     //     stateMachine->advanceAndApply(0.016f);
//     //     artboard->draw(renderer.get());
//     // }

//     CHECK(silver.matches("controlling_node_read_data_with_scripting"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/interpolation_zero_duration.riv",
//     &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = viewModelId == -1
//                    ? file->createViewModelInstance(artboard.get())
//                    : file->createViewModelInstance(viewModelId, 0);
//     auto objectX =
//     vmi->propertyValue("objectX")->as<ViewModelInstanceNumber>(); auto
//     interpValue =
//         vmi->propertyValue("interpValue")->as<ViewModelInstanceNumber>();

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     objectX->propertyValue(200);

//     int frames = (int)(1.5f / 0.1f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.1f);
//         artboard->draw(renderer.get());
//     }

//     interpValue->propertyValue(0);
//     stateMachine->advanceAndApply(0.016f);
//     objectX->propertyValue(400);
//     stateMachine->advanceAndApply(0.016f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.1f);
//         artboard->draw(renderer.get());
//     }

//     printf("interpValue back to 1\n");
//     interpValue->propertyValue(1);
//     stateMachine->advanceAndApply(0.016f);
//     objectX->propertyValue(200);
//     stateMachine->advanceAndApply(0.016f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.1f);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("interpolation_zero_duration"));
// // }
// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/bindable_artboard_nesty.riv", &silver);
//     auto child = ReadRiveFile("assets/bindable_artboard_child.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createViewModelInstance(artboard.get());

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     CHECK(silver.matches("bindable_artboard_nesty"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/remove_from_list.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = viewModelId == -1
//                    ? file->createViewModelInstance(artboard.get())
//                    : file->createViewModelInstance(viewModelId, 0);

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     int frames = (int)(1.0f / 0.2f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.2f);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("remove_from_list"));
// // }
// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/foocus_nodes_list_order.riv", &silver);

//     auto artboard = file->artboardDefault();
//     // auto artboard = file->artboardNamed("NestedArtboards");
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     auto renderer = silver.makeRenderer();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     auto focusManager = stateMachine->focusManager();
//     focusManager->focusNext();
//     silver.addFrame();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("foocus_nodes_list_order"));
// }
// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/fintechonboarding-v8-stateful.riv",
//     &silver);

//     // auto artboard = file->artboardDefault();
//     auto artboard = file->artboardNamed("MainDebug");
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     auto focusManager = stateMachine->focusManager();

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     auto renderer = silver.makeRenderer();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Focuses on first element of tree
//     focusManager->focusNext();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     int frames = (int)(2.0f / 0.5f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.5f);
//         artboard->draw(renderer.get());
//     }

//     // Focuses on first element of tree
//     focusManager->focusNext();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.5f);
//         artboard->draw(renderer.get());
//     }

//     // Focuses on first element of tree
//     focusManager->focusNext();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.5f);
//         artboard->draw(renderer.get());
//     }

//     // Focuses on first element of tree
//     focusManager->focusNext();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.5f);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("fintechonboarding-v8-stateful"));
// }
// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/genericmenus.riv", &silver);

//     // auto artboard = file->artboardDefault();
//     auto artboard = file->artboardNamed("Outfit");
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     auto focusManager = stateMachine->focusManager();

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     auto renderer = silver.makeRenderer();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     printf("==> First Pointer down\n");
//     stateMachine->pointerDown(Vec2D(85, 61));
//     stateMachine->advanceAndApply(0.016f);
//     stateMachine->pointerMove(Vec2D(429, 125));
//     stateMachine->advanceAndApply(0.016f);
//     printf("==> first Pointer up\n");
//     stateMachine->pointerUp(Vec2D(429, 125));
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     printf("==> Second Pointer down\n");
//     stateMachine->pointerDown(Vec2D(25, 61));
//     stateMachine->advanceAndApply(0.016f);
//     stateMachine->pointerMove(Vec2D(300, 100));
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     stateMachine->pointerMove(Vec2D(508, 125));
//     stateMachine->advanceAndApply(0.016f);
//     printf("==> Second Pointer up\n");
//     stateMachine->pointerUp(Vec2D(508, 125));
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // printf("==> First Pointer down\n");
//     // stateMachine->pointerDown(Vec2D(85, 61));
//     // stateMachine->advanceAndApply(0.016f);
//     // artboard->draw(renderer.get());
//     // silver.addFrame();
//     // stateMachine->pointerMove(Vec2D(385, 61));
//     // stateMachine->advanceAndApply(0.016f);
//     // artboard->draw(renderer.get());
//     // silver.addFrame();
//     // stateMachine->pointerUp(Vec2D(385, 61));
//     // stateMachine->advanceAndApply(0.016f);
//     // artboard->draw(renderer.get());
//     // silver.addFrame();
//     // printf("==> Second Pointer down\n");
//     // stateMachine->pointerDown(Vec2D(25, 61));
//     // stateMachine->advanceAndApply(0.016f);
//     // artboard->draw(renderer.get());
//     // silver.addFrame();
//     // stateMachine->pointerMove(Vec2D(385, 61));
//     // stateMachine->advanceAndApply(0.016f);
//     // artboard->draw(renderer.get());
//     // silver.addFrame();
//     // stateMachine->pointerUp(Vec2D(385, 61));
//     // stateMachine->advanceAndApply(0.016f);
//     // artboard->draw(renderer.get());
//     // silver.addFrame();

//     CHECK(silver.matches("genericmenus"));
// // }
// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/scripted_viewmodel_cache.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     auto focusManager = stateMachine->focusManager();

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     auto createInstanceProp =
//         vmi->propertyValue("createInstance")->as<ViewModelInstanceTrigger>();

//     auto renderer = silver.makeRenderer();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Source of the VM instance is now source-1
//     stateMachine->pointerDown(Vec2D(450, 50));
//     stateMachine->pointerUp(Vec2D(450, 50));
//     stateMachine->advanceAndApply(0.016f);

//     createInstanceProp->trigger();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Source of the VM instance is now source-2
//     stateMachine->pointerDown(Vec2D(450, 150));
//     stateMachine->pointerUp(Vec2D(450, 150));
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     createInstanceProp->trigger();
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("scripted_viewmodel_cache"));
// // }
// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/focus_traversal.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     auto focusManager = stateMachine->focusManager();

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     auto renderer = silver.makeRenderer();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // There are 2 rows of buttons
//     // Top row: Top / Right / Down / Left
//     // Bottom row: Prev / Next

//     // Click on Next
//     stateMachine->pointerDown(Vec2D(180, 450));
//     stateMachine->pointerUp(Vec2D(180, 450));
//     stateMachine->advanceAndApply(0.016f);
//     // Second advance to apply focus changes
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Click on Prev
//     stateMachine->pointerDown(Vec2D(60, 450));
//     stateMachine->pointerUp(Vec2D(60, 450));
//     stateMachine->advanceAndApply(0.016f);
//     // Second advance to apply focus changes
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Click on Up
//     stateMachine->pointerDown(Vec2D(60, 350));
//     stateMachine->pointerUp(Vec2D(60, 350));
//     stateMachine->advanceAndApply(0.016f);
//     // Second advance to apply focus changes
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Click on Left
//     stateMachine->pointerDown(Vec2D(420, 350));
//     stateMachine->pointerUp(Vec2D(420, 350));
//     stateMachine->advanceAndApply(0.016f);
//     // Second advance to apply focus changes
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Click on Down
//     stateMachine->pointerDown(Vec2D(300, 350));
//     stateMachine->pointerUp(Vec2D(300, 350));
//     stateMachine->advanceAndApply(0.016f);
//     // Second advance to apply focus changes
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     // Click on Right
//     stateMachine->pointerDown(Vec2D(180, 350));
//     stateMachine->pointerUp(Vec2D(180, 350));
//     stateMachine->advanceAndApply(0.016f);
//     // Second advance to apply focus changes
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("focus_traversal"));
// }
// TEST_CASE("Microprofile", "[silver]")
// {
// #if defined(RIVE_MICROPROFILE)
//     std::cerr
//         << "[Microprofile] RIVE_MICROPROFILE is ON (flag worked). CSV path: "
//         << std::filesystem::absolute("microprofile_Microprofile_test.csv")
//         << '\n';
// #else
//     std::fprintf(
//         stderr,
//         "[Microprofile] RIVE_MICROPROFILE is OFF — no CSV. Regenerate
//         makefiles/xcode " "with premake passing `--with_microprofile`, then
//         rebuild projects `rive` and "
//         "`unit_tests`.\n");
// #endif

//     rive::SerializingFactory silver;
//     auto file = ReadRiveFile("assets/chess_pvp_059_07.riv", &silver);
//     // auto file =
//     ReadRiveFile("assets/riv_processingloadtest_00_modified.riv",
//     // &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

// #if defined(RIVE_MICROPROFILE)
//     MicroProfileInit();
//     MicroProfileOnThreadCreate("MainThread");
//     MicroProfileSetEnableAllGroups(true);
//     MicroProfileSetForceEnable(true);
// #endif

//     auto renderer = silver.makeRenderer();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     RIVE_PROF_ENDFRAME();
//     silver.addFrame();
//     stateMachine->pointerDown(Vec2D(200.0f / 2.0f, 250.0f / 2.0f));
//     stateMachine->pointerUp(Vec2D(200.0f / 2.0f, 250.0f / 2.0f));
//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     RIVE_PROF_ENDFRAME();

//     int frames = (int)(10.0f / 0.016f);
//     // int frames = (int)(0.032f / 0.016f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.016f);
//         artboard->draw(renderer.get());
//         RIVE_PROF_ENDFRAME();
//     }

// #if defined(RIVE_MICROPROFILE)
//     {
//         // const char* csvPath =
//         "microprofile_Microprofile_modified_test.csv"; const char* csvPath =
//         "microprofile_Microprofile_test.csv"; const uint32_t profiledFrames =
//             static_cast<uint32_t>(frames + 1); // first frame + loop
//         MicroProfileDumpFile(csvPath, MicroProfileDumpTypeCsv,
//         profiledFrames); RIVE_PROF_ENDFRAME(); std::ifstream in(csvPath); if
//         (in.good())
//         {
//             std::cout << "---- MicroProfile CSV (" << csvPath << ") ----\n";
//             std::cout << in.rdbuf();
//             std::cout << "---- end MicroProfile CSV ----\n";
//         }
//         else
//         {
//             std::cerr << "[Microprofile] Failed to open \"" << csvPath
//                       << "\" after dump (cwd="
//                       << std::filesystem::current_path() << ").\n";
//         }
//     }
// #endif

//     CHECK(silver.matches("chess_pvp_059_07"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/focusable_element.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = viewModelId == -1
//                    ? file->createViewModelInstance(artboard.get())
//                    : file->createViewModelInstance(viewModelId, 0);

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     stateMachine->focusManager()->focusNext();
//     printf("==>> advance frame\n");
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     stateMachine->focusManager()->focusNext();
//     printf("==>> advance frame\n");
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     stateMachine->focusManager()->focusNext();
//     printf("==>> advance frame\n");
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());
//     silver.addFrame();
//     stateMachine->focusManager()->focusNext();
//     printf("==>> advance frame\n");
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("focusable_element"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     printf("=====================================> FILE 1\n");
//     auto file = ReadRiveFile("assets/avatar_v2_deliv_108.riv", &silver);
//     printf("=====================================> FILE 2\n");
//     auto file2 =
//         ReadRiveFile("assets/dlg.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createViewModelInstance(artboard.get());
//     auto renderer = silver.makeRenderer();

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.0f);

//     auto avatarArtboard = file2->bindableArtboardNamed("costume_artboard");

//     auto abProp = vmi->propertyValue("costume_db_artboard")
//                       ->as<ViewModelInstanceArtboard>();
//     abProp->asset(avatarArtboard);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     stateMachine->advanceAndApply(0.016f);
//     artboard->draw(renderer.get());
//     silver.addFrame();

//     CHECK(silver.matches("avatar_v2_deliv_108"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/scripted_interpolator.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     auto triProp = vmi->propertyValue("tri")->as<ViewModelInstanceTrigger>();

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     silver.addFrame();
//     stateMachine->advanceAndApply(0.016);
//     artboard->draw(renderer.get());

//     // int frames = (int)(2.0f / 0.032f);
//     int frames = (int)(0.08f / 0.032f);
//     for (int i = 0; i < frames; i++)
//     {
//         printf("==> NEW FRAME\n");
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.032f);
//         artboard->draw(renderer.get());
//     }
//     triProp->trigger();
//     stateMachine->advanceAndApply(0.016);
//     for (int i = 0; i < frames; i++)
//     {
//         printf("==> NEW FRAME\n");
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.032f);
//         artboard->draw(renderer.get());
//     }
//     printf("==> trigger\n");
//     triProp->trigger();
//     stateMachine->advanceAndApply(0.016);
//     for (int i = 0; i < frames; i++)
//     {
//         printf("==> NEW FRAME\n");
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.032f);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("scripted_interpolator"));
// }

// TEST_CASE("submitGamepadsFromBuffer accepts a connected record",
// TEST_CASE("XXXXX",
//           "[gamepad]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/gamepad_test.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     auto stateMachine = artboard->stateMachineAt(0);
//     REQUIRE(stateMachine != nullptr);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     stateMachine->bindViewModelInstance(vmi);
//     // Advance so the state machine initializes its focus manager / scripted
//     // drawable list before we dispatch any gamepad events.
//     stateMachine->advanceAndApply(0);

//     // Build the wire bytes the embedder (e.g. JS
//     `registerGamepadInteractions`
//     // with `GAMEPAD_BATCH_VERSION = 2`) would produce for a single
//     "connected"
//     // record. All multi-byte fields are little-endian, fixed-width — see the
//     // comment block in `gamepad_batch.cpp`:
//     //   uint32  version (= kGamepadBatchWireVersion)
//     //   uint8   GamepadRecordType::connected (0)
//     //   int32   deviceId
//     //   uint8   mapping (0 = standard, else unknown)
//     //   uint8   nButtons          (<= kGamepadBatchMaxButtons)
//     //   uint8   nAxes             (<= kGamepadBatchMaxAxes)
//     //   uint8   padding           (alignment filler)
//     //   float32 buttonValues[nButtons]
//     //   float32 axes[nAxes]
//     std::vector<uint8_t> buf;
//     auto pushU8 = [&](uint8_t v) { buf.push_back(v); };
//     auto pushU32 = [&](uint32_t v) {
//         buf.push_back(static_cast<uint8_t>(v));
//         buf.push_back(static_cast<uint8_t>(v >> 8));
//         buf.push_back(static_cast<uint8_t>(v >> 16));
//         buf.push_back(static_cast<uint8_t>(v >> 24));
//     };
//     auto pushI32 = [&](int32_t v) {
//         pushU32(static_cast<uint32_t>(v));
//     };
//     auto pushF32 = [&](float v) {
//         uint32_t bits;
//         std::memcpy(&bits, &v, sizeof(bits));
//         pushU32(bits);
//     };

//     // W3C "Standard Gamepad" layout: 17 buttons + 4 axes, all neutral. Any
//     // counts within `kGamepadBatchMax{Buttons,Axes}` are valid; we mimic a
//     // freshly-connected, untouched controller.
//     constexpr int32_t kDeviceId = 0;
//     constexpr uint8_t kNButtons = 17;
//     constexpr uint8_t kNAxes = 4;
//     static_assert(kNButtons <= kGamepadBatchMaxButtons, "button cap");
//     static_assert(kNAxes <= kGamepadBatchMaxAxes, "axis cap");

//     pushU32(kGamepadBatchWireVersion);
//     pushU8(static_cast<uint8_t>(GamepadRecordType::connected));
//     pushI32(kDeviceId);
//     pushU8(0); // mapping = standard
//     pushU8(kNButtons);
//     pushU8(kNAxes);
//     pushU8(0); // padding to align the float arrays
//     for (uint8_t b = 0; b < kNButtons; b++)
//     {
//         pushF32(0.f);
//     }
//     for (uint8_t a = 0; a < kNAxes; a++)
//     {
//         pushF32(0.f);
//     }

//     // Header (4) + record type (1) + deviceId (4) + 4 header bytes
//     // + 17 button floats + 4 axis floats = 13 + 17*4 + 4*4 = 97 bytes.
//     REQUIRE(buf.size() ==
//             4u + 1u + 4u + 4u + size_t(kNButtons) * 4u + size_t(kNAxes) *
//             4u);

//     CHECK(stateMachine->submitGamepadsFromBuffer(buf.data(), buf.size()));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file =
//         ReadRiveFile("assets/chess_board_db_2.riv", &silver);
//         // ReadRiveFile("assets/chess_board_db_1.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     silver.addFrame();
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());

//     // file = nullptr;

//     int frames = (int)(1.0f / 0.2f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.2f);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("chess_board_db_1"));
// }

// TEST_CASE("XXXXX", "[data binding]")
// {
//     // Note: the data_binding_artboards_source_test has a view model created
//     // that matches the view model the original artboards have. This is a
//     // temporary "hack" to validate that the artboard gets correctly bound
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/reused_artboard.riv", &silver);
//     auto sourceFile = ReadRiveFile("assets/reused_artboard.riv", &silver);

//     auto artboard = file->artboardDefault();
//     silver.frameSize(artboard->width(), artboard->height());

//     REQUIRE(artboard != nullptr);
//     auto stateMachine = artboard->stateMachineAt(0);
//     int viewModelId = artboard.get()->viewModelId();

//     auto vmi = viewModelId == -1
//                    ? file->createViewModelInstance(artboard.get())
//                    : file->createViewModelInstance(viewModelId, 0);

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     auto vmiArtboard =
//         vmi->propertyValue("ab")->as<ViewModelInstanceArtboard>();
//         vmiArtboard->useOriginalInstance(true);
//     silver.addFrame();
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());

//     printf("Updating AB\n");
//     auto ch1Source = file->bindableArtboardNamed("Child3");
//     vmiArtboard->asset(ch1Source);

//     silver.addFrame();
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());

//     int frames = 1.0f / 0.016f;
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.016f);
//         ch1Source->artboard()->advance(0.016);
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("reused_artboard"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/nested_artboard_solo_test.riv",
//     &silver); printf("====> AB default\n"); auto artboard =
//     file->artboardDefault(); REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     auto artboard_num_prop =
//     vmi->propertyValue("artboard_num")->as<ViewModelInstanceNumber>();
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());
//     printf("==> Set to 1\n");
//     artboard_num_prop->propertyValue(1);
//     silver.addFrame();
//     stateMachine->advanceAndApply(0.25f);
//     artboard->draw(renderer.get());

//     printf("==> Set to 0\n");
//     artboard_num_prop->propertyValue(0);
//     silver.addFrame();
//     stateMachine->advanceAndApply(0.25f);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("nested_artboard_solo_test"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/g4_proto2_slots_2.riv", &silver);
//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());
//     auto renderer = silver.makeRenderer();

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     auto CardContainer =
//     vmi->propertyValue("CardContainer")->as<ViewModelInstanceViewModel>();
//     printf("CardContainer: %p\n", CardContainer);
//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1);
//     artboard->draw(renderer.get());

//     silver.addFrame();
//     stateMachine->advanceAndApply(1);
//     artboard->draw(renderer.get());

//     CHECK(silver.matches("g4_proto2_slots_2"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file = ReadRiveFile("assets/g4_proto2_slots_2.riv", &silver);

//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());
//     auto CardContainerVM = vmi->propertyValue("CardContainer")
//                                ->as<ViewModelInstanceViewModel>()
//                                ->referenceViewModelInstance();
//     auto CardHandList = CardContainerVM->propertyValue("CardHandList")
//                             ->as<ViewModelInstanceList>();
//     auto cardVM = CardHandList->item(0)->viewModelInstance();
//     auto ULPositionX =
//         cardVM->propertyValue("ULPositionX")->as<ViewModelInstanceNumber>();
//     printf("ULPositionX: %f\n", ULPositionX->propertyValue());

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     silver.addFrame();
//     stateMachine->advanceAndApply(0.1f);
//     artboard->draw(renderer.get());
//     printf("ULPositionX: %f\n", ULPositionX->propertyValue());

//     // file = nullptr;

//     int frames = (int)(1.0f / 0.2f);
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.2f);
//         artboard->draw(renderer.get());
//         printf("ULPositionX: %f\n", ULPositionX->propertyValue());
//     }

//     CHECK(silver.matches("g4_proto2_slots_2"));
// }

// TEST_CASE("XXXXX", "[silver]")
// {
//     SerializingFactory silver;
//     auto file =
//         ReadRiveFile("assets/artboard_opacity_and_transform_test.riv",
//         &silver);
//     printf("================================================\n");
//     auto artboard = file->artboardDefault();
//     REQUIRE(artboard != nullptr);

//     silver.frameSize(artboard->width(), artboard->height());

//     auto stateMachine = artboard->stateMachineAt(0);

//     auto vmi = file->createDefaultViewModelInstance(artboard.get());

//     auto xPos = vmi->propertyValue("xPos")->as<ViewModelInstanceNumber>();
//     auto yPos = vmi->propertyValue("yPos")->as<ViewModelInstanceNumber>();

//     stateMachine->bindViewModelInstance(vmi);
//     stateMachine->advanceAndApply(0.1f);
//     auto renderer = silver.makeRenderer();
//     artboard->draw(renderer.get());

//     int frames = 11;
//     for (int i = 0; i < frames; i++)
//     {
//         silver.addFrame();
//         stateMachine->advanceAndApply(0.1f);

//         stateMachine->pointerDown(
//             rive::Vec2D(xPos->propertyValue(), yPos->propertyValue()));
//         stateMachine->pointerUp(
//             rive::Vec2D(xPos->propertyValue(), yPos->propertyValue()));
//         artboard->draw(renderer.get());
//     }

//     CHECK(silver.matches("artboard_opacity_and_transform_test"));
// }
