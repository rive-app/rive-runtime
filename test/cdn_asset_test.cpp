#include <rive/bones/skin.hpp>
#include <rive/bones/tendon.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/path_vertex.hpp>
#include <rive/shapes/points_path.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/assets/file_asset.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("Image assets with cdn information loads correctly", "[cdn]")
{
    auto file = ReadRiveFile("../../test/assets/hosted_image_file.riv");

    auto assets = file->assets();
    REQUIRE(assets.size() == 1);
    auto firstAsset = assets[0];
    REQUIRE(firstAsset->is<rive::ImageAsset>());

    // this is a 16 byte uuid, any good ideas on how to manage this test?
    // we could convert it to a string for the getter...
    REQUIRE(firstAsset->cdnUuid().size() == 16);
    REQUIRE(strcmp("edcb1816-8405-4983-acd2-16db48d85df4", firstAsset->cdnUuidStr().c_str()) == 0);
    REQUIRE(firstAsset->cdnBaseUrl() == "https://public.uat.rive.app/cdn/uuid");

    REQUIRE(firstAsset->uniqueFilename() == "one-45008.png");
    REQUIRE(firstAsset->fileExtension() == "png");
}

TEST_CASE("Font assets with cdn information loads correctly", "[cdn]")
{
    auto file = ReadRiveFile("../../test/assets/hosted_font_file.riv");

    auto assets = file->assets();
    REQUIRE(assets.size() == 1);
    auto firstAsset = assets[0];
    REQUIRE(firstAsset->is<rive::FontAsset>());

    // this is a 16 byte uuid, any good ideas on how to manage this test?
    // we could convert it to a string for the getter...
    REQUIRE(firstAsset->cdnUuid().size() == 16);
    REQUIRE(firstAsset->cdnBaseUrl() == "https://public.uat.rive.app/cdn/uuid");

    REQUIRE(firstAsset->uniqueFilename() == "Inter-43276.ttf");
    REQUIRE(firstAsset->fileExtension() == "ttf");
}
