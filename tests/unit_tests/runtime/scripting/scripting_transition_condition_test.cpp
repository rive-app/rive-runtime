
#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

TEST_CASE("Scripted transition condition", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/scripted_transition_condition.riv", &silver);
    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto tlBool =
        vmi->propertyValue("timelineBool")->as<ViewModelInstanceBoolean>();
    auto anyBool =
        vmi->propertyValue("anyStateBool")->as<ViewModelInstanceBoolean>();

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();

    tlBool->propertyValue(true);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();

    anyBool->propertyValue(true);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("scripted_transition_condition"));
}