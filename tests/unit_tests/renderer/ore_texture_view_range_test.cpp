/*
 * Copyright 2026 Rive
 */

// View ranges are checked once on the base context so every backend, the
// recorder and the script bindings agree on what a bad view is. GPU free.

#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

static rcp<Texture> makeTex(Context& ctx, TextureType type, uint32_t layers)
{
    TextureDesc td{};
    td.width = 16;
    td.height = 16;
    td.depthOrArrayLayers = layers;
    td.type = type;
    td.numMipmaps = 4;
    return ctx.makeTexture(td);
}

TEST_CASE("zero view counts span the rest of the texture", "[ore]")
{
    DeferredOreContext ctx(nullptr);
    auto tex = makeTex(ctx, TextureType::array2D, 3);

    TextureViewDesc vd{};
    vd.texture = tex.get();
    vd.dimension = TextureViewDimension::array2D;
    vd.baseMipLevel = 1;
    vd.mipCount = 0;
    vd.baseLayer = 1;
    vd.layerCount = 0;
    auto view = ctx.makeTextureView(vd);
    REQUIRE(view != nullptr);
    CHECK(view->mipCount() == 3);
    CHECK(view->layerCount() == 2);
}

TEST_CASE("view ranges past the texture are rejected with a message", "[ore]")
{
    DeferredOreContext ctx(nullptr);
    auto tex = makeTex(ctx, TextureType::array2D, 3);

    TextureViewDesc vd{};
    vd.texture = tex.get();
    vd.dimension = TextureViewDimension::array2D;

    SECTION("base level") { vd.baseMipLevel = 4; }
    SECTION("mip count")
    {
        vd.baseMipLevel = 2;
        vd.mipCount = 3;
    }
    SECTION("base layer") { vd.baseLayer = 3; }
    SECTION("layer count")
    {
        vd.baseLayer = 1;
        vd.layerCount = 3;
    }
    ctx.clearLastError();
    CHECK(ctx.makeTextureView(vd) == nullptr);
    CHECK_FALSE(ctx.lastError().empty());
}

TEST_CASE("view layers follow the texture type", "[ore]")
{
    DeferredOreContext ctx(nullptr);

    SECTION("a cube spans six faces whatever the desc depth says")
    {
        auto tex = makeTex(ctx, TextureType::cube, 1);
        TextureViewDesc vd{};
        vd.texture = tex.get();
        vd.dimension = TextureViewDimension::cube;
        vd.layerCount = 6;
        CHECK(ctx.makeTextureView(vd) != nullptr);
        vd.layerCount = 0;
        auto whole = ctx.makeTextureView(vd);
        REQUIRE(whole != nullptr);
        CHECK(whole->layerCount() == 6);
    }
    SECTION("a 3D texture spans one layer, not its depth")
    {
        auto tex = makeTex(ctx, TextureType::texture3D, 8);
        TextureViewDesc vd{};
        vd.texture = tex.get();
        vd.dimension = TextureViewDimension::texture3D;
        vd.layerCount = 0;
        auto whole = ctx.makeTextureView(vd);
        REQUIRE(whole != nullptr);
        CHECK(whole->layerCount() == 1);
        vd.layerCount = 8;
        CHECK(ctx.makeTextureView(vd) == nullptr);
    }
}
