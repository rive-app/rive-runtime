/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SERIALIZING_FACTORY_HPP_
#define _RIVE_SERIALIZING_FACTORY_HPP_

#include "rive/factory.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace rive
{
namespace gpu
{
class RenderContext;
class RenderCanvas;
} // namespace gpu

// A factory that generates render objects which serialize their rendering
// commands into one buffer that can then be used to replay the commands in a
// viewer app or compare them to ensure that subsequent runs generate the same
// commands.
//
// It is also a DeferredCanvasHost, which is what lets a [silver] test exercise
// cache-as-bitmap: an artboard with a BitmapCache child only rasterizes
// offscreen when its factory answers both renderContext() and
// deferredCanvasHost(). Off by default so existing goldens are untouched --
// call enableBitmapCache() to turn it on.
class SerializingFactory : public Factory, public cmd::DeferredCanvasHost
{
public:
    SerializingFactory();
    ~SerializingFactory() override;

    // Wires the two hooks Artboard::drawCachedAsBitmap requires. The artboard
    // uses renderContext only to mint the offscreen canvas
    // (makeDeferredRenderCanvas), which touches no device, so a null-device
    // context such as RenderContextNULL::MakeContext() is enough. Canvas
    // content then records inline into this factory's stream, bracketed by
    // canvasContentBegin/End, and the composite draws the canvas id.
    void enableBitmapCache(gpu::RenderContext* renderContext);

    Factory* renderContext() override;
    cmd::DeferredCanvasHost* deferredCanvasHost() override;
    cmd::DeferredCanvasHost* canvasContentHost() override;

    // ---- DeferredCanvasHost ----
    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas,
                                 uint32_t clearColor) override;
    void endCanvasContent(gpu::RenderCanvas* canvas) override;
    rcp<gpu::RenderCanvas> makeContentCanvas(uint32_t width,
                                             uint32_t height) override;
    rcp<RenderImage> contentCanvasImage(gpu::RenderCanvas* canvas) override;

    // Stream id for a drawable image. Canvas-backed images are registered
    // here; everything else carries its own id.
    uint64_t imageId(const RenderImage*) const;

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    rcp<RenderPath> makeEmptyRenderPath() override;

    rcp<RenderPaint> makeRenderPaint() override;

    rcp<RenderImage> decodeImage(Span<const uint8_t>) override;

    std::unique_ptr<Renderer> makeRenderer();

    void addFrame();
    void frameSize(uint32_t width, uint32_t height);

    void save(const char* filename);
    bool matches(const char* filename);

    // Recorded SRIV stream for replay via serialized_replay.hpp.
    Span<const uint8_t> bytes() const
    {
        return Span<const uint8_t>(m_buffer.data(), m_buffer.size());
    }

private:
    void saveTarnished(const char* filename);
    // Allocates and records the canvas's id on first sight, then returns it.
    uint64_t canvasId(gpu::RenderCanvas* canvas);

    std::vector<uint8_t> m_buffer;
    VectorBinaryWriter m_writer;

    // Non-null once enableBitmapCache() has run.
    gpu::RenderContext* m_bitmapCacheContext = nullptr;
    // Canvas-backed images live in the same id space as decoded ones so a
    // composite is just an ordinary drawImage. Keyed by pointer, so every
    // canvas ever registered is retained below: a canvas that died could
    // otherwise have its address recycled by the next one and inherit its id,
    // which makes the recording depend on the allocator.
    std::unordered_map<const RenderImage*, uint64_t> m_canvasImageIds;
    std::vector<rcp<gpu::RenderCanvas>> m_retainedCanvases;
    std::unique_ptr<Renderer> m_canvasRenderer;

    uint64_t m_renderImageId = 0;
    uint64_t m_renderPaintId = 0;
    uint64_t m_renderPathId = 0;
    uint64_t m_renderBufferId = 0;
    uint64_t m_renderShaderId = 0;
};
} // namespace rive
#endif
