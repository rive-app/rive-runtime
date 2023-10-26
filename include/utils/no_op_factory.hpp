/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_NOOP_FACTORY_HPP_
#define _RIVE_NOOP_FACTORY_HPP_

#include "rive/factory.hpp"

namespace rive
{

class NoOpFactory : public Factory
{
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

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

    std::unique_ptr<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    std::unique_ptr<RenderPath> makeEmptyRenderPath() override;

    std::unique_ptr<RenderPaint> makeRenderPaint() override;

    rcp<RenderImage> decodeImage(Span<const uint8_t>) override;
};
} // namespace rive
#endif
