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

TEST_CASE("Dynamic image binding with listener action", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/image_binding_with_listener.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    auto renderer = silver.makeRenderer();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(650.0f, 650.0f));
    stateMachine->pointerUp(rive::Vec2D(650.0f, 650.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    auto imageFile = ReadFile("assets/open_source.jpg");
    REQUIRE(imageFile.size() == 8880);

    auto decodedImage = silver.decodeImage(imageFile);
    auto imgProp =
        vmi->propertyValue("image1")->as<rive::ViewModelInstanceAssetImage>();
    imgProp->value(decodedImage.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(650.0f, 650.0f));
    stateMachine->pointerUp(rive::Vec2D(650.0f, 650.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    imgProp->value(nullptr);

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->pointerDown(rive::Vec2D(650.0f, 650.0f));
    stateMachine->pointerUp(rive::Vec2D(650.0f, 650.0f));
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("image_binding_with_listener"));
}

TEST_CASE("Image fit & alignment with databound images", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/image_fit_alignment.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    // Coverage for generated ImageBase fit/alignment setters/getters.
    auto firstImage = [&]() -> rive::Image* {
        for (auto image : artboard->objects<rive::Image>())
        {
            return image;
        }
        return nullptr;
    };
    auto imageForCoverage = firstImage();
    REQUIRE(imageForCoverage != nullptr);
    auto originalFit = imageForCoverage->fit();
    auto originalAlignmentX = imageForCoverage->alignmentX();
    auto originalAlignmentY = imageForCoverage->alignmentY();

    auto testFit = originalFit == static_cast<uint32_t>(rive::Fit::contain)
                       ? static_cast<uint32_t>(rive::Fit::cover)
                       : static_cast<uint32_t>(rive::Fit::contain);
    auto testAlignmentX = originalAlignmentX == -1.0f ? 1.0f : -1.0f;
    auto testAlignmentY = originalAlignmentY == -1.0f ? 1.0f : -1.0f;

    imageForCoverage->fit(testFit);
    imageForCoverage->alignmentX(testAlignmentX);
    imageForCoverage->alignmentY(testAlignmentY);

    CHECK(imageForCoverage->fit() == testFit);
    CHECK(imageForCoverage->alignmentX() == testAlignmentX);
    CHECK(imageForCoverage->alignmentY() == testAlignmentY);

    imageForCoverage->fit(originalFit);
    imageForCoverage->alignmentX(originalAlignmentX);
    imageForCoverage->alignmentY(originalAlignmentY);

    CHECK(imageForCoverage->fit() == originalFit);
    CHECK(imageForCoverage->alignmentX() == originalAlignmentX);
    CHECK(imageForCoverage->alignmentY() == originalAlignmentY);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto imageProperty = vmi->propertyValue("imageProperty");
    REQUIRE(imageProperty != nullptr);
    REQUIRE(imageProperty->is<rive::ViewModelInstanceAssetImage>());
    auto imageAssetProperty =
        imageProperty->as<rive::ViewModelInstanceAssetImage>();
    auto noScaleImage = [&]() -> rive::Image* {
        for (auto image : artboard->objects<rive::Image>())
        {
            if (image->fit() == static_cast<uint32_t>(rive::Fit::none))
            {
                return image;
            }
        }
        return nullptr;
    };

    auto assets = file->assets();
    auto findAssetIndexByName = [&](const char* assetName) -> size_t {
        for (size_t i = 0; i < assets.size(); i++)
        {
            if (assets[i]->name() == assetName)
            {
                return i;
            }
        }
        return assets.size();
    };

    auto image1Index = findAssetIndexByName("image1");
    auto image2Index = findAssetIndexByName("image2");
    auto image3Index = findAssetIndexByName("image3");
    REQUIRE(image1Index != assets.size());
    REQUIRE(image2Index != assets.size());
    REQUIRE(image3Index != assets.size());

    int frames = 20;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    imageAssetProperty->propertyValue(static_cast<uint32_t>(image2Index));
    stateMachine->advanceAndApply(0.0f);
    auto noScale = noScaleImage();
    REQUIRE(noScale != nullptr);
    CHECK(noScale->transform()[4] < 0.0f);
    CHECK(noScale->transform()[5] < 0.0f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    imageAssetProperty->propertyValue(static_cast<uint32_t>(image3Index));
    stateMachine->advanceAndApply(0.0f);
    noScale = noScaleImage();
    REQUIRE(noScale != nullptr);
    CHECK(noScale->transform()[4] < 0.0f);
    CHECK(noScale->transform()[5] < 0.0f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("image_fit_alignment"));
}

TEST_CASE("Image fit & alignment with databound images 2", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/image_fit_alignment_2.riv", &silver);

    auto artboard = file->artboardNamed("Main");
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

    int frames = 60;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("image_fit_alignment_2"));
}
