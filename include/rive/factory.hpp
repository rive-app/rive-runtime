/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_FACTORY_HPP_
#define _RIVE_FACTORY_HPP_

#include "rive/renderer.hpp"
#include "rive/text_engine.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/math/aabb.hpp"

#include <stdio.h>
#include <cstdint>

namespace rive
{

class RawPath;
namespace ore
{
class Context;
}
namespace cmd
{
class DeferredCanvasHost;
}

class Factory
{
public:
    Factory() {}
    virtual ~Factory() {}

    virtual rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                               RenderBufferFlags,
                                               size_t sizeInBytes) = 0;

    virtual rcp<RenderShader> makeLinearGradient(
        float sx,
        float sy,
        float ex,
        float ey,
        const ColorInt colors[], // [count]
        const float stops[],     // [count]
        size_t count) = 0;

    virtual rcp<RenderShader> makeRadialGradient(
        float cx,
        float cy,
        float radius,
        const ColorInt colors[], // [count]
        const float stops[],     // [count]
        size_t count) = 0;

    // Returns a full-formed RenderPath -- can be treated as immutable
    // This call might swap out the arrays backing the points and verbs in the
    // given RawPath, so the caller can expect it to be in an undefined state
    // upon return.
    virtual rcp<RenderPath> makeRenderPath(RawPath&, FillRule) = 0;

    // Deprecated -- working to make RenderPath's immutable
    virtual rcp<RenderPath> makeEmptyRenderPath() = 0;

    virtual rcp<RenderPaint> makeRenderPaint() = 0;

    virtual rcp<RenderImage> decodeImage(Span<const uint8_t>) = 0;

    // GPU ore context, when this factory is backed by a RenderContext.
    // Null for non-GPU factories. Kept last in the virtual section to avoid
    // shifting existing vtable slots.
    virtual ore::Context* ore() { return nullptr; }

    // The GPU render context an import through this factory should give its
    // scripts, as a Factory so this header stays free of gpu types. A render
    // context answers with itself; a recording session answers with the one it
    // records for, which on web is null until a render texture attaches, so
    // callers that deferred an allocation ask again rather than caching the
    // null they saw at import. Null means the importer cannot route GPU
    // scripting.
    virtual Factory* renderContext() { return nullptr; }

    // Set when script canvas work must record rather than issue. Null means
    // scripts draw straight to the driver.
    virtual cmd::DeferredCanvasHost* deferredCanvasHost() { return nullptr; }

    rcp<Font> decodeFont(Span<const uint8_t>);

    rcp<AudioSource> decodeAudio(Span<const uint8_t>);

    // Non-virtual helpers

    rcp<RenderPath> makeRenderPath(const AABB&);
};

} // namespace rive
#endif
