#include <rive/assets/image_asset.hpp>
#include <rive/shapes/image.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

TEST_CASE("image sampler resolves asset defaults and node overrides", "[image]")
{
    auto file = ReadRiveFile("assets/tape.riv");
    auto node = file->artboard()->find("Tape body.png");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Image>());
    auto image = node->as<rive::Image>();
    auto asset = image->imageAsset();
    REQUIRE(asset != nullptr);

    REQUIRE(image->imageSampler() == rive::ImageSampler::LinearClamp());

    asset->samplerFilter((uint32_t)rive::ImageFilter::nearest);
    asset->samplerWrapX((uint32_t)rive::ImageWrap::repeat);
    asset->samplerWrapY((uint32_t)rive::ImageWrap::mirror);
    auto fromAsset = image->imageSampler();
    REQUIRE(fromAsset.filter == rive::ImageFilter::nearest);
    REQUIRE(fromAsset.wrapX == rive::ImageWrap::repeat);
    REQUIRE(fromAsset.wrapY == rive::ImageWrap::mirror);

    // Node values are offset by one, zero inherits from the asset.
    image->samplerFilter(1 + (uint32_t)rive::ImageFilter::bilinear);
    image->samplerWrapX(1 + (uint32_t)rive::ImageWrap::clamp);
    auto overridden = image->imageSampler();
    REQUIRE(overridden.filter == rive::ImageFilter::bilinear);
    REQUIRE(overridden.wrapX == rive::ImageWrap::clamp);
    REQUIRE(overridden.wrapY == rive::ImageWrap::mirror);

    // Malformed file values fall back to safe defaults.
    image->samplerFilter(200);
    image->samplerWrapX(200);
    auto clamped = image->imageSampler();
    REQUIRE(clamped.filter == rive::ImageFilter::bilinear);
    REQUIRE(clamped.wrapX == rive::ImageWrap::clamp);
}
