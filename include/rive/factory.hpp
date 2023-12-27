/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_FACTORY_HPP_
#define _RIVE_FACTORY_HPP_

#include "rive/renderer.hpp"
#include "rive/text_engine.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/math/aabb.hpp"

#include <stdio.h>
#include <cstdint>

namespace rive
{

class RawPath;

class Factory
{
public:
    Factory() {}
    virtual ~Factory() {}

    virtual rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                               RenderBufferFlags,
                                               size_t sizeInBytes) = 0;

    virtual rcp<RenderShader> makeLinearGradient(float sx,
                                                 float sy,
                                                 float ex,
                                                 float ey,
                                                 const ColorInt colors[], // [count]
                                                 const float stops[],     // [count]
                                                 size_t count) = 0;

    virtual rcp<RenderShader> makeRadialGradient(float cx,
                                                 float cy,
                                                 float radius,
                                                 const ColorInt colors[], // [count]
                                                 const float stops[],     // [count]
                                                 size_t count) = 0;

    // Returns a full-formed RenderPath -- can be treated as immutable
    // This call might swap out the arrays backing the points and verbs in the given RawPath, so the
    // caller can expect it to be in an undefined state upon return.
    virtual rcp<RenderPath> makeRenderPath(RawPath&, FillRule) = 0;

    // Deprecated -- working to make RenderPath's immutable
    virtual rcp<RenderPath> makeEmptyRenderPath() = 0;

    virtual rcp<RenderPaint> makeRenderPaint() = 0;

    virtual rcp<RenderImage> decodeImage(Span<const uint8_t>) = 0;

    virtual rcp<Font> decodeFont(Span<const uint8_t>);

    // Non-virtual helpers

    rcp<RenderPath> makeRenderPath(const AABB&);
};

} // namespace rive
#endif
