/*
 * Copyright 2026 Rive
 */

// Instancing exists so N copies of a mesh cost one draw. A backend that lost
// the instance count, or a path that quietly fell back to one draw per
// instance, still looks right on screen. These tests count batches and
// instances rather than looking at pixels.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "assets/nomoon.png.hpp"
#include <catch.hpp>

using namespace rive;
using namespace rive::gpu;

namespace
{
constexpr uint32_t InstanceCount = 5;

class CapturingRenderContext : public RenderContextNULL
{
public:
    size_t meshBatches = 0;
    uint32_t meshElements = 0;

    void flush(const FlushDescriptor& desc) override
    {
        if (desc.drawList == nullptr)
        {
            return;
        }
        for (const DrawBatch& batch : *desc.drawList)
        {
            if (batch.drawType == DrawType::imageMesh)
            {
                ++meshBatches;
                meshElements += batch.elementCount;
            }
        }
    }
};

struct Harness
{
    std::unique_ptr<RenderContext> ctx;
    CapturingRenderContext* capture;
    rcp<RenderImage> image;
    rcp<RenderBuffer> pts, uvs, indices;

    Harness()
    {
        auto impl = std::make_unique<CapturingRenderContext>();
        capture = impl.get();
        ctx = std::make_unique<RenderContext>(std::move(impl));
        ctx->beginFrame({
            .renderTargetWidth = 100,
            .renderTargetHeight = 100,
        });

        image = ctx->decodeImage(assets::nomoon_png());

        const Vec2D quad[4] = {{0, 0}, {50, 0}, {50, 50}, {0, 50}};
        const Vec2D uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
        pts = makeBuffer(RenderBufferType::vertex, quad, sizeof(quad));
        uvs = makeBuffer(RenderBufferType::vertex, uv, sizeof(uv));
        indices = makeBuffer(RenderBufferType::index, idx, sizeof(idx));
    }

    rcp<RenderBuffer> makeBuffer(RenderBufferType type,
                                 const void* source,
                                 size_t size)
    {
        auto buffer =
            ctx->makeRenderBuffer(type,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  size);
        memcpy(buffer->map(), source, size);
        buffer->unmap();
        return buffer;
    }

    void flush()
    {
        auto target = capture->makeRenderTarget(100, 100);
        ctx->flush({.renderTarget = target.get()});
    }
};
} // namespace

TEST_CASE("an instanced mesh draw is one batch", "[RiveRenderer][mesh]")
{
    Harness h;
    REQUIRE(h.image != nullptr);

    auto instances = h.ctx->makeImageMeshInstances(InstanceCount);
    Span<ImageMeshInstanceData> data = instances->edit();
    for (uint32_t i = 0; i < InstanceCount; ++i)
    {
        data[i].transform = Mat2D::fromTranslate(i * 10.0f, 0);
        data[i].opacity = 1.0f - 0.1f * i;
    }
    instances->endEdit();

    RiveRenderer renderer(h.ctx.get());
    renderer.drawImageMeshInstanced(h.image.get(),
                                    ImageSampler::LinearClamp(),
                                    h.pts,
                                    h.uvs,
                                    h.indices,
                                    4,
                                    6,
                                    instances);
    h.flush();

    CHECK(h.capture->meshBatches == 1);
    CHECK(h.capture->meshElements == InstanceCount);
}

TEST_CASE("an empty instance array draws nothing", "[RiveRenderer][mesh]")
{
    Harness h;
    REQUIRE(h.image != nullptr);

    auto instances = h.ctx->makeImageMeshInstances(0);
    instances->edit();
    instances->endEdit();

    RiveRenderer renderer(h.ctx.get());
    renderer.drawImageMeshInstanced(h.image.get(),
                                    ImageSampler::LinearClamp(),
                                    h.pts,
                                    h.uvs,
                                    h.indices,
                                    4,
                                    6,
                                    instances);
    h.flush();

    CHECK(h.capture->meshBatches == 0);
    CHECK(h.capture->meshElements == 0);
}

TEST_CASE("separate mesh draws are a batch each", "[RiveRenderer][mesh]")
{
    Harness h;
    REQUIRE(h.image != nullptr);

    RiveRenderer renderer(h.ctx.get());
    for (uint32_t i = 0; i < InstanceCount; ++i)
    {
        renderer.save();
        renderer.transform(Mat2D::fromTranslate(i * 10.0f, 0));
        renderer.drawImageMesh(h.image.get(),
                               ImageSampler::LinearClamp(),
                               h.pts,
                               h.uvs,
                               h.indices,
                               4,
                               6,
                               BlendMode::srcOver,
                               1.0f);
        renderer.restore();
    }
    h.flush();

    CHECK(h.capture->meshBatches == InstanceCount);
    CHECK(h.capture->meshElements == InstanceCount);
}
