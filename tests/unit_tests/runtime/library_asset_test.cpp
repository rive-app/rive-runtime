#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include <rive/animation/nested_simple_animation.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/relative_local_asset_loader.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("File with library artboard loads", "[libraries]")
{
    // created by test in rive_core.
    auto file = ReadRiveFile("assets/library_export_test.riv");

    auto nestedArtboard =
        file->artboard()->find<rive::NestedArtboard>("The nested artboard");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->name() == "The nested artboard");
    REQUIRE(nestedArtboard->x() == 1);
    REQUIRE(nestedArtboard->y() == 2);
    REQUIRE(nestedArtboard->artboardId() == -1);

    // time to find the library asset somewhere
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);
    // todo assert nested artboard structure
}

TEST_CASE("File with library animation loads", "[libraries]")
{
    // created by test in rive_core.
    auto file = ReadRiveFile("assets/library_export_animation_test.riv");

    auto nestedArtboard =
        file->artboard()->find<rive::NestedArtboard>("The nested artboard");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->name() == "The nested artboard");
    REQUIRE(nestedArtboard->x() == 1);
    REQUIRE(nestedArtboard->y() == 2);
    REQUIRE(nestedArtboard->artboardId() == -1);

    auto nestedSimpleAnimation =
        file->artboard()->find<rive::NestedSimpleAnimation>("");
    REQUIRE(nestedSimpleAnimation != nullptr);
    REQUIRE(nestedSimpleAnimation->name() == "");
    REQUIRE(nestedSimpleAnimation->isPlaying() == true);
    // // TODO: figure out what the id should be?
    REQUIRE(nestedSimpleAnimation->animationId() == -1);

    // // time to find the library asset somewhere
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);

    REQUIRE(nestedArtboard->nestedAnimations().size() == 1);
    // todo assert nested artboard has animation structure
}

TEST_CASE("File with library state machine loads", "[libraries]")
{
    // created by test in rive_core.
    auto file = ReadRiveFile("assets/library_export_state_machine_test.riv");

    auto nestedArtboard =
        file->artboard()->find<rive::NestedArtboard>("The nested artboard");
    REQUIRE(nestedArtboard != nullptr);
    REQUIRE(nestedArtboard->name() == "The nested artboard");
    REQUIRE(nestedArtboard->x() == 1);
    REQUIRE(nestedArtboard->y() == 2);
    REQUIRE(nestedArtboard->artboardId() == -1);

    auto nestedSimpleAnimation =
        file->artboard()->find<rive::NestedStateMachine>("");
    REQUIRE(nestedSimpleAnimation != nullptr);
    REQUIRE(nestedSimpleAnimation->name() == "");
    REQUIRE(nestedSimpleAnimation->animationId() == -1);

    // time to find the library asset somewhere
    // make sure its loaded?
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);

    REQUIRE(nestedArtboard->nestedAnimations().size() == 1);
    // todo assert nested artboard has state machine structure
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
    // TODO: create this file in the editor tests.
    auto file = ReadRiveFile("assets/library_with_image.riv");

    // library, image and text
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);
    // TODO: asset image
}

TEST_CASE("File with multiple libraries including image", "[libraries]")
{
    // TODO: create this file in the editor tests.
    auto file = ReadRiveFile("assets/double_library_with_image.riv");

    // library, image and text
    auto assets = file->assets();
    REQUIRE(assets.size() == 0);
    // TODO: asset multiple images
}
