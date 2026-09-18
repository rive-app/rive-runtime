/*
 * Copyright 2026 Rive
 */

// Upload regions are checked once on the base texture so every backend, the
// recorder and the script bindings agree on what a bad upload is. GPU free.

#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"

#include <catch.hpp>
#include <vector>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

static rcp<Texture> makeTex(Context& ctx,
                            TextureType type,
                            uint32_t layers,
                            uint32_t mips = 1)
{
    TextureDesc td{};
    td.width = 8;
    td.height = 8;
    td.depthOrArrayLayers = layers;
    td.type = type;
    td.numMipmaps = mips;
    return ctx.makeTexture(td);
}

TEST_CASE("zero upload fields fill from the mip level", "[ore]")
{
    DeferredOreContext ctx(nullptr);
    auto tex = makeTex(ctx, TextureType::texture2D, 1, 2);
    std::vector<uint8_t> pixels(8 * 8 * 4);

    TextureDataDesc d{};
    d.data = pixels.data();
    d.dataSize = static_cast<uint32_t>(pixels.size());
    d.mipLevel = 1;
    std::string error;
    CHECK(tex->upload(d, &error));
    CHECK(error.empty());

    SECTION("from an offset the rest of the level remains")
    {
        d.mipLevel = 0;
        d.x = 6;
        d.y = 6;
        CHECK(tex->upload(d, &error));
    }
    SECTION("a short buffer is refused")
    {
        d.mipLevel = 0;
        d.dataSize = 8;
        CHECK_FALSE(tex->upload(d, &error));
        CHECK(error.find("needs") != std::string::npos);
    }
}

TEST_CASE("upload regions outside the texture are rejected", "[ore]")
{
    DeferredOreContext ctx(nullptr);
    auto tex = makeTex(ctx, TextureType::texture2D, 1, 2);
    std::vector<uint8_t> pixels(8 * 8 * 4);

    TextureDataDesc d{};
    d.data = pixels.data();

    SECTION("mip level") { d.mipLevel = 2; }
    SECTION("layer") { d.layer = 1; }
    SECTION("origin") { d.x = 8; }
    SECTION("extent")
    {
        d.x = 4;
        d.width = 8;
    }
    SECTION("row stride")
    {
        d.width = 4;
        d.bytesPerRow = 8;
    }
    SECTION("rows per image")
    {
        d.height = 4;
        d.rowsPerImage = 2;
    }
    std::string error;
    CHECK_FALSE(tex->upload(d, &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("upload layers and depth follow the texture type", "[ore]")
{
    DeferredOreContext ctx(nullptr);
    std::vector<uint8_t> pixels(8 * 8 * 8 * 4);

    SECTION("a cube face uploads whatever the desc depth says")
    {
        auto tex = makeTex(ctx, TextureType::cube, 1);
        TextureDataDesc d{};
        d.data = pixels.data();
        d.layer = 5;
        CHECK(tex->upload(d));
        d.layer = 6;
        CHECK_FALSE(tex->upload(d));
    }
    SECTION("a 3D texture spans its depth in z, not in layers")
    {
        auto tex = makeTex(ctx, TextureType::texture3D, 8);
        TextureDataDesc d{};
        d.data = pixels.data();
        d.z = 2;
        d.depth = 0;
        CHECK(tex->upload(d));
        d.depth = 8;
        CHECK_FALSE(tex->upload(d));
        d.depth = 1;
        d.layer = 1;
        CHECK_FALSE(tex->upload(d));
    }
}
