#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/image.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/constraints/follow_path_constraint.hpp>
#include <rive/viewmodel/viewmodel_instance_artboard.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("Test data binding artboards from same and different sources",
          "[data binding]")
{
    // Note: the data_binding_artboards_source_test has a view model created
    // that matches the view model the original artboards have. This is a
    // temporary "hack" to validate that the artboard gets correctly bound
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_binding_artboards_test.riv", &silver);
    auto sourceFile =
        ReadRiveFile("assets/data_binding_artboards_source_test.riv", &silver);

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

    auto vmiArtboard =
        vmi->propertyValue("ab")->as<ViewModelInstanceArtboard>();
    auto vmiChild = vmi->propertyValue("ch")
                        ->as<ViewModelInstanceViewModel>()
                        ->referenceViewModelInstance()
                        .get();
    auto vmTrigger =
        vmiChild->propertyValue("tr")->as<ViewModelInstanceTrigger>();
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change source to local artboard at index 1

    vmiArtboard->propertyValue(1);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change source to local artboard at index 2

    vmiArtboard->propertyValue(2);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change source to external artboard "source_1"
    auto source1 = sourceFile->artboard("source_1");
    vmiArtboard->asset(source1);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Fire trigger once to change text color
    vmTrigger->trigger();
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Fire trigger once to change text color again
    vmTrigger->trigger();
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change source to external artboard "source_2"
    auto source2 = sourceFile->artboard("source_2");
    vmiArtboard->asset(source2);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change source to external artboard "source_1"
    vmiArtboard->asset(source1);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    // Change back to local artboard at index 1
    vmiArtboard->propertyValue(2);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("data_binding_artboards_test"));
}
