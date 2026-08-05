/*
 * Copyright 2026 Rive
 */

// An artboard instanced with an override factory routes all instance level
// render resource creation through it, nested instances included; the file
// level factory keeps only the shared decode products.

#include <rive/artboard.hpp>
#include <rive/factory.hpp>
#include <rive/nested_artboard.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"

#include <catch.hpp>

using namespace rive;

namespace
{
// Counts creations, delegates through the base so no op objects come back.
class CountingFactory : public Factory
{
public:
    int paints = 0;
    int paths = 0;
    int buffers = 0;
    int shaders = 0;

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType type,
                                       RenderBufferFlags flags,
                                       size_t size) override
    {
        ++buffers;
        return inner().makeRenderBuffer(type, flags, size);
    }
    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        ++shaders;
        return inner().makeLinearGradient(sx, sy, ex, ey, colors, stops, count);
    }
    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        ++shaders;
        return inner().makeRadialGradient(cx, cy, radius, colors, stops, count);
    }
    rcp<RenderPath> makeRenderPath(RawPath& path, FillRule rule) override
    {
        ++paths;
        return inner().makeRenderPath(path, rule);
    }
    rcp<RenderPath> makeEmptyRenderPath() override
    {
        ++paths;
        return inner().makeEmptyRenderPath();
    }
    rcp<RenderPaint> makeRenderPaint() override
    {
        ++paints;
        return inner().makeRenderPaint();
    }
    rcp<RenderImage> decodeImage(Span<const uint8_t> bytes) override
    {
        return inner().decodeImage(bytes);
    }

private:
    // NoOpFactory's overrides are private; the base class view is public.
    Factory& inner() { return m_inner; }
    NoOpFactory m_inner;
};
} // namespace

TEST_CASE("instance without an override keeps the file factory",
          "[instance_factory]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    auto instance = file->artboard()->instance<ArtboardInstance>();
    REQUIRE(instance != nullptr);
    REQUIRE(instance->factory() == file->artboard()->factory());
}

TEST_CASE("instance override reroutes resource creation", "[instance_factory]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    CountingFactory facade;
    auto instance = file->artboard()->instance<ArtboardInstance>(&facade);
    REQUIRE(instance != nullptr);
    REQUIRE(instance->factory() == &facade);
    // Fill and stroke paints are created during instancing.
    REQUIRE(facade.paints > 0);
}

TEST_CASE("nested instances inherit the override factory", "[instance_factory]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    CountingFactory facade;
    auto instance = file->artboard()->instance<ArtboardInstance>(&facade);
    REQUIRE(instance != nullptr);

    auto nested = instance->find<NestedArtboard>("Nested artboard container");
    REQUIRE(nested != nullptr);
    REQUIRE(nested->sourceArtboard() != nullptr);
    REQUIRE(nested->sourceArtboard()->factory() == &facade);
}

TEST_CASE("advance and draw allocate nothing on the file factory after an "
          "override instance",
          "[instance_factory]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    CountingFactory facade;
    auto instance = file->artboard()->instance<ArtboardInstance>(&facade);
    REQUIRE(instance != nullptr);

    instance->advance(0.016f);
    NoOpRenderer renderer;
    instance->draw(&renderer);
    // Lazily created shape paths land on the facade, not the file factory.
    REQUIRE(facade.paths > 0);
}
