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
#include <rive/viewmodel/viewmodel_instance_asset_image.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("Test data binding images from file assets", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_images_test.riv");

    auto artboard = file->artboard("main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    // View model properties
    // Images
    auto mainProperty = viewModelInstance->propertyValue("main_im");
    REQUIRE(mainProperty != nullptr);
    REQUIRE(mainProperty->is<rive::ViewModelInstanceAssetImage>());
    auto mainImgProperty =
        mainProperty->as<rive::ViewModelInstanceAssetImage>();
    auto sub1VMIProperty = viewModelInstance->propertyValue("sub_1");
    REQUIRE(sub1VMIProperty != nullptr);
    REQUIRE(sub1VMIProperty->is<rive::ViewModelInstanceViewModel>());
    auto referencedViewModel =
        sub1VMIProperty->as<rive::ViewModelInstanceViewModel>()
            ->referenceViewModelInstance();
    auto sub1Property = referencedViewModel->propertyValue("sub_1_im");
    REQUIRE(sub1Property != nullptr);
    REQUIRE(sub1Property->is<rive::ViewModelInstanceAssetImage>());
    auto sub1ImgProperty =
        sub1Property->as<rive::ViewModelInstanceAssetImage>();

    // File assets
    auto assets = file->assets();
    // Image layers

    REQUIRE(artboard->find<rive::Image>("root_img") != nullptr);
    auto rootImage = artboard->find<rive::Image>("root_img");

    REQUIRE(artboard->find<rive::NestedArtboard>("sub_1") != nullptr);
    auto nestedArtboardSub1 = artboard->find<rive::NestedArtboard>("sub_1");

    auto nestedArtboardArtboardSub1 = nestedArtboardSub1->artboardInstance();
    REQUIRE(nestedArtboardArtboardSub1 != nullptr);
    REQUIRE(nestedArtboardArtboardSub1->find<rive::Image>("sub_1_img") !=
            nullptr);
    auto sub1Image = nestedArtboardArtboardSub1->find<rive::Image>("sub_1_img")
                         ->as<rive::Image>();
    REQUIRE(sub1Image != nullptr);
    // Validations
    // Ensure view model image asset is the same as the image's image asset
    auto mainAsset = assets[mainImgProperty->propertyValue()].get();
    auto imageAsset = rootImage->imageAsset();
    REQUIRE(imageAsset == mainAsset);
    auto sub1Asset = assets[sub1ImgProperty->propertyValue()].get();
    auto sub1ImageAsset = sub1Image->imageAsset();
    REQUIRE(sub1Asset == sub1ImageAsset);
    // Change values
    mainImgProperty->propertyValue(2);
    sub1ImgProperty->propertyValue(6);
    artboard->advance(0.0f);
    auto updatedMainAsset = assets[mainImgProperty->propertyValue()].get();
    auto updatedSub1Asset = assets[sub1ImgProperty->propertyValue()].get();
    // Ensure image is no longer the same
    REQUIRE(imageAsset != updatedMainAsset);
    REQUIRE(sub1ImageAsset != updatedSub1Asset);
    // Ensure new image asset is the one assigned to the view model property
    // asset
    imageAsset = rootImage->imageAsset();
    REQUIRE(imageAsset == updatedMainAsset);
    sub1ImageAsset = sub1Image->imageAsset();
    REQUIRE(sub1ImageAsset == updatedSub1Asset);
}

TEST_CASE("Embedded images can be reset by passing null", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/viewmodel_image_reset.riv", &silver);

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

    auto vmiImage =
        vmi->propertyValue("img")->as<ViewModelInstanceAssetImage>();
    vmiImage->value(nullptr);
    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("viewmodel_image_reset"));
}

TEST_CASE("Image based conditions work", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/viewmodel_based_condition.riv", &silver);

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

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(1.1f);
    // Since these tests are relying on an event, we need to advance ne extra
    // time for the event to be processed
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(1.1f);
    // Since these tests are relying on an event, we need to advance ne extra
    // time for the event to be processed
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("viewmodel_based_condition"));
}