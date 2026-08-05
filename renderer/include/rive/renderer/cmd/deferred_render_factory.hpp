/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/renderer/cmd/foreign_image_registry.hpp"
#include "rive/renderer/cmd/id_allocator.hpp"
#include "rive/renderer/cmd/deferred_render_resource.hpp"
#include "rive/renderer/cmd/render_command_buffer.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include <cassert>
#include <cstdio>
#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

// DeferredFactory and DeferredRenderer are the 2D recording front end. make*
// assigns a dense id and records a creation command; draws reference resources
// by id. Nothing touches the GPU; the render side replays the single ordered
// stream against a real Factory and Renderer.
namespace rive::cmd
{

// Cheap encoded image dimension sniff. Builds without RIVE_DECODERS still
// need record time width and height for layout. Defined in
// src/deferred_cmd.cpp.
bool sniffImageSize(Span<const uint8_t> b, int& w, int& h);

class DeferredFactory : public Factory
{
public:
    DeferredFactory()
    {
        // Stream order consumes each create destroy pair, so a recycled id
        // never aliases a live one.
        registerRecorder(&m_buffer);
    }

    ~DeferredFactory()
    {
        // Unregister first so resources a straggling wrapper still holds no-op
        // their release instead of writing into a dead recorder.
        unregisterRecorder(&m_buffer);
        // Apply destroys queued from other threads before the stream dies.
        m_buffer.drainDestroys();
    }

    // Creates, mutations, draws, and destroys all record into one ordered
    // stream so a single replay pass suffices and id reuse stays correct.
    std::unique_ptr<Renderer> makeRenderer(
        ForeignImageRegistry* canvases = nullptr);
    const RenderCommandBuffer& commandBuffer() const { return m_buffer; }
    RenderCommandBuffer& commandBuffer() { return m_buffer; }

    // Clear the ordered stream for the next frame; consumer keeps resources.
    void resetFrame()
    {
        m_buffer.reset();
        // Drain cross thread GC destroys into the new frame's stream head.
        m_buffer.drainDestroys();
    }

    rcp<RenderPath> makeRenderPath(RawPath& path, FillRule fr) override
    {
        auto a = m_pathIds.alloc();
        auto verbs = path.verbs();
        auto points = path.points();
        uint64_t verbsOff = m_buffer.appendBlob(
            verbs.data(),
            static_cast<uint32_t>(verbs.size() * sizeof(PathVerb)));
        uint64_t pointsOff = m_buffer.appendBlob(
            points.data(),
            static_cast<uint32_t>(points.size() * sizeof(Vec2D)));
        m_buffer.append(static_cast<uint8_t>(RenderCmd::makePath),
                        MakePathPOD{a.id,
                                    a.generation,
                                    verbsOff,
                                    pointsOff,
                                    static_cast<uint32_t>(verbs.size()),
                                    static_cast<uint32_t>(points.size()),
                                    static_cast<uint32_t>(fr)});
        return make_rcp<DeferredRenderPath>(a.id,
                                            a.generation,
                                            &m_buffer,
                                            &m_pathIds);
    }

    rcp<RenderPath> makeEmptyRenderPath() override
    {
        auto a = m_pathIds.alloc();
        m_buffer.append(static_cast<uint8_t>(RenderCmd::makeEmptyPath),
                        MakeIdPOD{a.id, a.generation});
        return make_rcp<DeferredRenderPath>(a.id,
                                            a.generation,
                                            &m_buffer,
                                            &m_pathIds);
    }

    rcp<RenderPaint> makeRenderPaint() override
    {
        auto a = m_paintIds.alloc();
        m_buffer.append(static_cast<uint8_t>(RenderCmd::makePaint),
                        MakeIdPOD{a.id, a.generation});
        return make_rcp<DeferredRenderPaint>(a.id,
                                             a.generation,
                                             &m_buffer,
                                             &m_paintIds);
    }

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        auto a = m_shaderIds.alloc();
        GradientBlobs g = appendGradientStops(colors, stops, count);
        m_buffer.append(static_cast<uint8_t>(RenderCmd::makeLinearGradient),
                        LinearGradientPOD{a.id,
                                          a.generation,
                                          sx,
                                          sy,
                                          ex,
                                          ey,
                                          g.colorsOffset,
                                          g.stopsOffset,
                                          static_cast<uint32_t>(count)});
        return make_rcp<DeferredRenderShader>(a.id,
                                              a.generation,
                                              &m_buffer,
                                              &m_shaderIds);
    }
    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        auto a = m_shaderIds.alloc();
        GradientBlobs g = appendGradientStops(colors, stops, count);
        m_buffer.append(static_cast<uint8_t>(RenderCmd::makeRadialGradient),
                        RadialGradientPOD{a.id,
                                          a.generation,
                                          cx,
                                          cy,
                                          radius,
                                          static_cast<uint32_t>(count),
                                          g.colorsOffset,
                                          g.stopsOffset});
        return make_rcp<DeferredRenderShader>(a.id,
                                              a.generation,
                                              &m_buffer,
                                              &m_shaderIds);
    }

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType type,
                                       RenderBufferFlags flags,
                                       size_t sizeInBytes) override
    {
        auto a = m_bufferIds.alloc();
        m_buffer.append(static_cast<uint8_t>(RenderCmd::makeBuffer),
                        MakeBufferPOD{a.id,
                                      a.generation,
                                      static_cast<uint8_t>(type),
                                      static_cast<uint8_t>(flags),
                                      static_cast<uint32_t>(sizeInBytes)});
        // The resident buffer keeps its data across frames on the consumer.
        return make_rcp<DeferredRenderBuffer>(a.id,
                                              a.generation,
                                              type,
                                              flags,
                                              sizeInBytes,
                                              &m_buffer,
                                              &m_bufferIds);
    }

    rcp<RenderImage> decodeImage(Span<const uint8_t> bytes) override
    {
        auto a = m_imageIds.alloc();
        // Decode dims at record time so the artboard can read them during
        // advance; the render side uploads the real texture.
        int w = 0, h = 0;
#ifdef RIVE_DECODERS
        if (auto bm = Bitmap::decode(bytes.data(), bytes.size()))
        {
            w = static_cast<int>(bm->width());
            h = static_cast<int>(bm->height());
        }
#endif
        // wasm builds decode in the browser, so sniff dims from the header.
        if (w == 0 || h == 0)
        {
            sniffImageSize(bytes, w, h);
        }
#ifndef NDEBUG
        // Unknown dims silently break layout far from here, so warn once.
        if (w == 0 || h == 0)
        {
            static bool warned = false;
            if (!warned)
            {
                warned = true;
                fprintf(
                    stderr,
                    "DeferredFactory::decodeImage: image dims unknown at "
                    "record time (decode failed, or built without "
                    "RIVE_DECODERS); size-dependent layout will be wrong\n");
            }
        }
#endif
        uint64_t off = m_buffer.appendBlob(bytes.data(),
                                           static_cast<uint32_t>(bytes.size()));
        m_buffer.append(static_cast<uint8_t>(RenderCmd::decodeImage),
                        DecodeImagePOD{a.id,
                                       a.generation,
                                       off,
                                       static_cast<uint32_t>(bytes.size()),
                                       static_cast<uint32_t>(w),
                                       static_cast<uint32_t>(h)});
        return make_rcp<DeferredRenderImage>(a.id,
                                             a.generation,
                                             w,
                                             h,
                                             &m_buffer,
                                             &m_imageIds);
    }

private:
    // colors[count] (ColorInt) and stops[count] (float), each its own blob.
    struct GradientBlobs
    {
        uint64_t colorsOffset;
        uint64_t stopsOffset;
    };
    GradientBlobs appendGradientStops(const ColorInt colors[],
                                      const float stops[],
                                      size_t count)
    {
        GradientBlobs g;
        g.colorsOffset = m_buffer.appendBlob(
            colors,
            static_cast<uint32_t>(count * sizeof(ColorInt)));
        g.stopsOffset =
            m_buffer.appendBlob(stops,
                                static_cast<uint32_t>(count * sizeof(float)));
        return g;
    }

    RenderCommandBuffer
        m_buffer; // ordered: creates + mutations + draws + destroys
    // One reusable id space per resource type (Dawn-style free list + gen).
    IdAllocator<RenderHandle> m_pathIds;
    IdAllocator<RenderHandle> m_paintIds;
    IdAllocator<RenderHandle> m_shaderIds;
    IdAllocator<RenderHandle> m_imageIds;
    IdAllocator<RenderHandle> m_bufferIds;
};

// Draws are attributed to the renderer that issues them, not their stream
// position, so scripts can interleave canvases and the screen freely.
class DeferredRouteHost
{
public:
    virtual void routeTo(uint64_t target) = 0;

protected:
    ~DeferredRouteHost() = default;
};
// Canvases and screens share one route target space: a canvas uses its
// unflagged canvas id, a screen sets this flag over its target id. One session
// records for every screen target its render context drives, so a screen needs
// an identity a canvas id cannot alias.
constexpr uint64_t kScreenTargetFlag = 1ull << 63;
constexpr uint64_t kScreenTarget = kScreenTargetFlag; // screen target 0
constexpr uint64_t screenTarget(uint64_t id) { return kScreenTargetFlag | id; }
constexpr bool isScreenTarget(uint64_t target)
{
    return (target & kScreenTargetFlag) != 0;
}
constexpr uint64_t screenTargetId(uint64_t target)
{
    return target & ~kScreenTargetFlag;
}

class DeferredRenderer : public Renderer
{
public:
    explicit DeferredRenderer(RenderCommandBuffer* buffer,
                              ForeignImageRegistry* canvases = nullptr,
                              DeferredRouteHost* routeHost = nullptr,
                              uint64_t routeTarget = kScreenTarget) :
        m_buffer(buffer),
        m_canvases(canvases),
        m_routeHost(routeHost),
        m_routeTarget(routeTarget)
    {}

    void save() override
    {
        route();
        m_buffer->appendType(static_cast<uint8_t>(RenderCmd::save));
    }
    void restore() override
    {
        route();
        m_buffer->appendType(static_cast<uint8_t>(RenderCmd::restore));
    }
    void transform(const Mat2D& m) override
    {
        route();
        m_buffer->append(
            static_cast<uint8_t>(RenderCmd::transform),
            TransformPOD{m.xx(), m.xy(), m.yx(), m.yy(), m.tx(), m.ty()});
    }
    void drawPath(RenderPath* path, RenderPaint* paint) override
    {
        DeferredRenderPath::flushScratchOf(path);
        RenderHandle pathId = DeferredRenderPath::idOfPath(path);
        RenderHandle paintId = idOfPaint(paint);
        if (pathId == kInvalidRenderHandle || paintId == kInvalidRenderHandle)
        {
            // A foreign draw is dropped at replay anyway; recording it would
            // flood the stream every frame.
            warnForeign("drawPath");
            return;
        }
        auto* dp = lite_rtti_cast<DeferredRenderPath*>(path);
        auto* dpt = lite_rtti_cast<DeferredRenderPaint*>(paint);
        dp->markDrawn();
        dpt->markDrawn();
        route();
        m_buffer->append(
            static_cast<uint8_t>(RenderCmd::drawPath),
            DrawPathPOD{pathId, paintId, dp->version(), dpt->version()});
    }
    void clipPath(RenderPath* path) override
    {
        DeferredRenderPath::flushScratchOf(path);
        auto* dp = lite_rtti_cast<DeferredRenderPath*>(path);
        if (dp != nullptr)
        {
            dp->markDrawn();
        }
        route();
        m_buffer->append(static_cast<uint8_t>(RenderCmd::clipPath),
                         ClipPathPOD{DeferredRenderPath::idOfPath(path),
                                     dp != nullptr ? dp->version() : 0});
    }
    void modulateOpacity(float opacity) override
    {
        route();
        m_buffer->append(static_cast<uint8_t>(RenderCmd::modulateOpacity),
                         OpacityPOD{opacity});
    }

    void drawImage(const RenderImage* image,
                   ImageSampler s,
                   BlendMode blend,
                   float opacity) override
    {
        // Canvas images get a flagged id from the registry on first sight.
        RenderHandle id = idOfImage(image);
        if (id == kInvalidRenderHandle && m_canvases)
        {
            id = m_canvases->imageDrawId(const_cast<RenderImage*>(image));
        }
        if (id == kInvalidRenderHandle)
        {
            // Foreign image is dropped at replay anyway, skip recording.
            warnForeign("drawImage");
            return;
        }
        route();
        m_buffer->append(static_cast<uint8_t>(RenderCmd::drawImage),
                         DrawImagePOD{id,
                                      static_cast<uint8_t>(s.wrapX),
                                      static_cast<uint8_t>(s.wrapY),
                                      static_cast<uint8_t>(s.filter),
                                      static_cast<uint8_t>(blend),
                                      opacity});
    }
    void drawImageMesh(const RenderImage* image,
                       ImageSampler s,
                       rcp<RenderBuffer> vertices,
                       rcp<RenderBuffer> uvCoords,
                       rcp<RenderBuffer> indices,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode blend,
                       float opacity) override
    {
        RenderHandle imgId = idOfImage(image);
        if (imgId == kInvalidRenderHandle && m_canvases)
        {
            // A real image decoded outside the session (the host's decode
            // wrap makes that legitimate) rides to replay via the registry.
            imgId = m_canvases->imageDrawId(const_cast<RenderImage*>(image));
        }
        RenderHandle vId = idOfBuffer(vertices.get());
        RenderHandle uvId = idOfBuffer(uvCoords.get());
        RenderHandle idxId = idOfBuffer(indices.get());
        bool foreign =
            imgId == kInvalidRenderHandle || vId == kInvalidRenderHandle ||
            uvId == kInvalidRenderHandle || idxId == kInvalidRenderHandle;
        if (foreign)
        {
            // Foreign mesh is dropped at replay anyway, skip recording.
            warnForeign("drawImageMesh");
            return;
        }
        auto* dv = lite_rtti_cast<DeferredRenderBuffer*>(vertices.get());
        auto* duv = lite_rtti_cast<DeferredRenderBuffer*>(uvCoords.get());
        auto* di = lite_rtti_cast<DeferredRenderBuffer*>(indices.get());
        dv->markDrawn();
        duv->markDrawn();
        di->markDrawn();
        route();
        m_buffer->append(static_cast<uint8_t>(RenderCmd::drawImageMesh),
                         DrawImageMeshPOD{imgId,
                                          vId,
                                          uvId,
                                          idxId,
                                          dv->version(),
                                          duv->version(),
                                          di->version(),
                                          vertexCount,
                                          indexCount,
                                          static_cast<uint8_t>(s.wrapX),
                                          static_cast<uint8_t>(s.wrapY),
                                          static_cast<uint8_t>(s.filter),
                                          static_cast<uint8_t>(blend),
                                          opacity});
    }

private:
    // A foreign resource means the caller mixed factories; that is always a
    // bug worth surfacing.
    static void warnForeign(const char* what)
    {
        static int warned = 0;
        if (warned < 16)
        {
            warned = warned + 1;
            fprintf(stderr,
                    "rive deferred: %s with a foreign resource (made by a "
                    "different factory), draw will be dropped\n",
                    what);
        }
    }

    static RenderHandle idOfPaint(const RenderPaint* p)
    {
        auto* d =
            lite_rtti_cast<DeferredRenderPaint*>(const_cast<RenderPaint*>(p));
        return d ? d->id() : kInvalidRenderHandle;
    }
    static RenderHandle idOfImage(const RenderImage* i)
    {
        auto* d =
            lite_rtti_cast<DeferredRenderImage*>(const_cast<RenderImage*>(i));
        return d ? d->id() : kInvalidRenderHandle;
    }
    static RenderHandle idOfBuffer(const RenderBuffer* b)
    {
        auto* d =
            lite_rtti_cast<DeferredRenderBuffer*>(const_cast<RenderBuffer*>(b));
        return d ? d->id() : kInvalidRenderHandle;
    }
    // Attribute the coming op to this recorder's target; standalone recorders
    // have no host and skip it.
    void route()
    {
        if (m_routeHost != nullptr)
        {
            m_routeHost->routeTo(m_routeTarget);
        }
    }

    RenderCommandBuffer* m_buffer;
    ForeignImageRegistry* m_canvases;
    DeferredRouteHost* m_routeHost;
    uint64_t m_routeTarget;
};

inline std::unique_ptr<Renderer> DeferredFactory::makeRenderer(
    ForeignImageRegistry* canvases)
{
    return std::make_unique<DeferredRenderer>(&m_buffer, canvases);
}

} // namespace rive::cmd
