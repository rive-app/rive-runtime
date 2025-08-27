#include <rive/custom_property_string.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include <rive/animation/nested_simple_animation.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/relative_local_asset_loader.hpp>
#include <rive/shapes/image.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("File with library artboard loads", "[libraries]")
{
    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_export_test.riv");

    // Libraries artboards are always exported after host file artboards.
    // In this file, the first artboard (id=0) is the host file's Artboard,
    // and the second one came from the library.
    auto nestedArtboard =
        file->artboard(0)->find<rive::NestedArtboard>("The nested artboard");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->name() == "The nested artboard");
    REQUIRE(nestedArtboard->x() == 1);
    REQUIRE(nestedArtboard->y() == 2);
    REQUIRE(nestedArtboard->artboardId() == 1);

    auto artboard = file->artboard(1);
    REQUIRE(artboard->name() == "Rocket");
    REQUIRE(artboard->width() == 512);
    REQUIRE(artboard->height() == 513);

    // The library asset is not exported to runtime.
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);
}

TEST_CASE("File with library animation loads", "[libraries]")
{
    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_export_animation_test.riv");

    auto nestedArtboard =
        file->artboard(0)->find<rive::NestedArtboard>("The nested artboard");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->name() == "The nested artboard");
    REQUIRE(nestedArtboard->x() == 1);
    REQUIRE(nestedArtboard->y() == 2);

    REQUIRE(nestedArtboard->artboardId() == 1);
    REQUIRE(file->artboard(1)->animationCount() == 1);
    auto simpleAnimation = file->artboard(1)->firstAnimation();
    REQUIRE(simpleAnimation->name() == "LA Rocket");

    auto nestedAnimations = nestedArtboard->nestedAnimations();
    REQUIRE(nestedAnimations.size() == 1);
    auto nestedSimpleAnimation = nestedAnimations[0];
    REQUIRE(nestedSimpleAnimation->name() == "");

    REQUIRE(nestedSimpleAnimation->animationId() == 0);
    REQUIRE(nestedArtboard->nestedAnimations().size() == 1);

    // The library asset is not exported to runtime.
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);
}

TEST_CASE("File with library state machine loads", "[libraries]")
{
    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_export_state_machine_test.riv");

    auto nestedArtboard =
        file->artboard(0)->find<rive::NestedArtboard>("The nested artboard");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->name() == "The nested artboard");
    REQUIRE(nestedArtboard->x() == 1);
    REQUIRE(nestedArtboard->y() == 2);

    REQUIRE(nestedArtboard->artboardId() == 1);
    REQUIRE(file->artboard(1)->stateMachineCount() == 1);
    auto stateMachine = file->artboard(1)->firstStateMachine();
    REQUIRE(stateMachine->name() == "SM Rocket");

    auto nestedAnimations = nestedArtboard->nestedAnimations();
    REQUIRE(nestedAnimations.size() == 1);
    auto nestedStateMachine = nestedAnimations[0];
    REQUIRE(nestedStateMachine->name() == "");

    REQUIRE(nestedStateMachine->animationId() == 0);
    REQUIRE(nestedArtboard->nestedAnimations().size() == 1);

    // time to find the library asset somewhere
    // make sure its loaded?
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);
}

// TODO reinstate, but right now we're missing fonts locally, need to get
// backend to uat to recreate this test. we are literally just loading files
// here. TEST_CASE("File with library", "[libraries]")
// {
//     TODO: create this file in the editor tests.
//     auto file = ReadRiveFile("assets/library_with_text_and_image.riv");

//     // library, image and text
//     auto assets = file->assets();
//     REQUIRE(assets.size() == 3);
//     auto libraryAsset = assets[0];
//     REQUIRE(libraryAsset->is<rive::LibraryAsset>());
//     REQUIRE(libraryAsset->name() == "library - Version 1");

//     auto fontAsset = assets[1];
//     REQUIRE(fontAsset->is<rive::FontAsset>());
//     REQUIRE(fontAsset->name() == "Inter");

//     auto imageAsset = assets[2];
//     REQUIRE(imageAsset->is<rive::ImageAsset>());
//     REQUIRE(imageAsset->name() == "tree");

//     // library, image and text
//     auto libraryAssets =
//         static_cast<rive::LibraryAsset*>(libraryAsset)->riveFile()->assets();
//     REQUIRE(libraryAssets.size() == 2);

//     auto librayFontAsset = libraryAssets[0];
//     REQUIRE(librayFontAsset->is<rive::FontAsset>());
//     REQUIRE(librayFontAsset->name() == "Inter");

//     auto libraryImageAsset = libraryAssets[1];
//     REQUIRE(libraryImageAsset->is<rive::ImageAsset>());
//     REQUIRE(libraryImageAsset->name() == "tree");
// }

// we are literally just loading files here.
TEST_CASE("File with library including image", "[libraries]")
{
    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_with_image.riv");

    auto assets = file->assets();
    REQUIRE(assets.size() == 1);

    auto nestedArtboard =
        file->artboard(0)->find<rive::NestedArtboard>("The instance");
    REQUIRE(nestedArtboard != nullptr);

    auto sourceArtboard = nestedArtboard->sourceArtboard();
    REQUIRE(sourceArtboard != nullptr);

    auto images = sourceArtboard->find<rive::Image>();
    REQUIRE(images.size() == 1);
    REQUIRE(images[0]->assetId() == 0);
    REQUIRE(images[0]->imageAsset() != nullptr);
}

TEST_CASE("File with multiple libraries including image", "[libraries]")
{
    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/double_library_with_image.riv");

    auto assets = file->assets();
    REQUIRE(assets.size() == 2);

    auto nestedArtboard1 =
        file->artboard(0)->find<rive::NestedArtboard>("The nested artboard");
    auto nestedArtboard2 = file->artboard(0)->find<rive::NestedArtboard>(
        "Another nested artboard");
    REQUIRE(nestedArtboard1 != nullptr);
    REQUIRE(nestedArtboard2 != nullptr);

    auto sourceArtboard1 = nestedArtboard1->sourceArtboard();
    REQUIRE(sourceArtboard1 != nullptr);
    auto images1 = sourceArtboard1->find<rive::Image>();
    REQUIRE(images1.size() == 1);
    REQUIRE(images1[0]->imageAsset() != nullptr);
    REQUIRE(images1[0]->imageAsset()->name() == "MyFirstImageAsset");

    auto sourceArtboard2 = nestedArtboard2->sourceArtboard();
    REQUIRE(sourceArtboard2 != nullptr);
    auto images2 = sourceArtboard2->find<rive::Image>();
    REQUIRE(images2.size() == 1);
    REQUIRE(images2[0]->imageAsset() != nullptr);
    REQUIRE(images2[0]->imageAsset()->name() == "MyOtherImageAsset");
}

TEST_CASE("File with DataEnum", "[libraries]")
{
    // This test verifies that a .rev file with LibraryComponents can
    // export a .riv that has valid DataBind's that reference a DataEnum
    // coming from a library.

    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_data_enum_test.riv");
    auto artboard = file->artboard(0)->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    auto event = artboard->find<rive::Event>("my_event");
    REQUIRE(event != nullptr);

    artboard->advance(0.0f);

    auto customPropertyString =
        artboard->find<rive::CustomPropertyString>("my_event_property");
    REQUIRE(customPropertyString != nullptr);

    REQUIRE(customPropertyString->propertyValue() == "red3");
}

TEST_CASE("File with ViewModel", "[libraries]")
{
    // This test verifies that a .rev file with LibraryComponents can
    // export a .riv that has valid DataBind's that reference a ViewModel
    // coming from a library.

    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_view_model_test.riv");
    auto artboard = file->artboard(0)->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    // There's only 1 nested artboard.
    auto nestedArtboard = artboard->find<rive::NestedArtboard>("");
    REQUIRE(nestedArtboard != nullptr);

    auto libArtboard2 = nestedArtboard->artboardInstance();
    REQUIRE(libArtboard2->name() == "2");

    auto nestedArtboard2 = libArtboard2->find<rive::NestedArtboard>("");
    auto libArtboard1 = nestedArtboard2->artboardInstance();
    REQUIRE(libArtboard1->name() == "1");

    artboard->advance(0.0f);

    // There are 5 property types tested.
    // If any of these works, then the ViewModel property must have worked
    // String property works
    auto customPropertyString1 =
        libArtboard1->find<rive::CustomPropertyString>("for_string");
    REQUIRE(customPropertyString1 != nullptr);
    REQUIRE(customPropertyString1->propertyValue() == "hello");

    // Enum property works
    auto customPropertyString2 =
        libArtboard1->find<rive::CustomPropertyString>("for_enum");
    REQUIRE(customPropertyString2->propertyValue() == "uk");

    // Number property works
    auto rect = libArtboard1->find<rive::Rectangle>("");
    REQUIRE(rect != nullptr);
    REQUIRE(rect->width() == 123);
    REQUIRE(rect->height() == 123);

    // Color property works
    auto rectShape = libArtboard1->find<rive::Shape>("");
    REQUIRE(rectShape->children()[1]->is<rive::Fill>());
    rive::Fill* rectFill = rectShape->children()[1]->as<rive::Fill>();
    REQUIRE(rectFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 10, 15, 66));
}

TEST_CASE("library_vmtest_1_host", "[libraries]")
{
    // This test verifies that a .rev file with LibraryComponents can
    // export a .riv that has valid DataBind's that reference a ViewModel
    // and Artboard coming from two different libraries.

    // created by library_import_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/library_vmtest_1_host.riv");
    auto artboard = file->artboard(0)->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);

    // There's only 1 nested artboard.
    auto nestedArtboard = artboard->find<rive::NestedArtboard>("");
    REQUIRE(nestedArtboard != nullptr);

    auto lib2Artboard = nestedArtboard->artboardInstance();
    REQUIRE(lib2Artboard->name() == "lib2artboard");

    artboard->advance(0.0f);

    // Color property works
    auto paints = lib2Artboard->shapePaints();
    REQUIRE(paints.size() == 1);
    auto paint = paints[0];
    REQUIRE(paint->is<rive::Fill>());
    rive::Fill* fill = paint->as<rive::Fill>();
    REQUIRE(fill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 16, 21, 102));
}
