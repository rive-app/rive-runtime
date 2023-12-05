#include <rive/bones/skin.hpp>
#include <rive/bones/tendon.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/span.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/path_vertex.hpp>
#include <rive/shapes/points_path.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/assets/file_asset.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

class PretendAssetLoader : public rive ::FileAssetLoader
{

public:
    rive::FileAsset* attemptedAsset;

    bool loadContents(rive::FileAsset& asset,
                      rive::Span<const uint8_t> inBandBytes,
                      rive::Factory* factory) override
    {
        attemptedAsset = &asset;
        return true;
    }
};

class RejectAssetLoader : public rive ::FileAssetLoader
{

public:
    rive::FileAsset* attemptedAsset;

    bool loadContents(rive::FileAsset& asset,
                      rive::Span<const uint8_t> inBandBytes,
                      rive::Factory* factory) override
    {
        return false;
    }
};

TEST_CASE("Load asset with in-band image", "[asset]")
{
    auto file = ReadRiveFile("../../test/assets/in_band_asset.riv");

    auto assets = file->assets();
    REQUIRE(assets.size() == 1);
    auto firstAsset = assets[0];
    REQUIRE(firstAsset->is<rive::ImageAsset>());

    // in band asset, no cdn uuid set
    REQUIRE(firstAsset->cdnUuid().size() == 0);

    // default value
    REQUIRE(firstAsset->cdnBaseUrl() == "https://public.rive.app/cdn/uuid");

    REQUIRE(firstAsset->uniqueFilename() == "1x1-45022.png");
    REQUIRE(firstAsset->fileExtension() == "png");

    // we load in band assets, so the decoded size >0
    REQUIRE(firstAsset->as<rive::ImageAsset>()->decodedByteSize == 308);
}

TEST_CASE("Load asset with in-band image, passing responsibility to loader", "[asset]")
{
    auto loader = PretendAssetLoader();

    // our Loader has not attempted to load any asset.
    REQUIRE(loader.attemptedAsset == nullptr);

    auto file = ReadRiveFile("../../test/assets/in_band_asset.riv", nullptr, &loader);

    auto assets = file->assets();
    REQUIRE(assets.size() == 1);
    auto firstAsset = assets[0];
    REQUIRE(firstAsset->is<rive::ImageAsset>());

    // in band asset, no cdn uuid set
    REQUIRE(firstAsset->cdnUuid().size() == 0);

    // default value
    REQUIRE(firstAsset->cdnBaseUrl() == "https://public.rive.app/cdn/uuid");

    REQUIRE(firstAsset->uniqueFilename() == "1x1-45022.png");
    REQUIRE(firstAsset->fileExtension() == "png");

    // we do not load in band assets, so the decoded size is still 0
    REQUIRE(firstAsset->as<rive::ImageAsset>()->decodedByteSize == 0);

    // however our FileAssetLoader had a chance to load this asset.
    // in band asset, no cdn uuid set
    REQUIRE(loader.attemptedAsset->cdnUuid().size() == 0);

    // default value
    REQUIRE(loader.attemptedAsset->cdnBaseUrl() == "https://public.rive.app/cdn/uuid");

    REQUIRE(loader.attemptedAsset->uniqueFilename() == "1x1-45022.png");
    REQUIRE(loader.attemptedAsset->fileExtension() == "png");
}

TEST_CASE("Load asset with in-band image, rejecting the loading responsiblity as loader", "[asset]")
{
    auto loader = RejectAssetLoader();
    auto file = ReadRiveFile("../../test/assets/in_band_asset.riv", nullptr, &loader);
    auto assets = file->assets();
    auto firstAsset = assets[0];
    REQUIRE(firstAsset->is<rive::ImageAsset>());
    // our loader does not handle loading the asset, so we load the in band contents.
    REQUIRE(firstAsset->as<rive::ImageAsset>()->decodedByteSize == 308);
}
