//
// Copyright 2022 Rive
//

#ifndef _RIVE_SOKOL_FACTORY_HPP_
#define _RIVE_SOKOL_FACTORY_HPP_

#include "rive/factory.hpp"

namespace rive
{

class SokolFactory : public Factory
{

public:
    SokolFactory();

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

    // Returns a full-formed RenderPath -- can be treated as immutable
    rcp<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    rcp<RenderPath> makeEmptyRenderPath() override;

    rcp<RenderPaint> makeRenderPaint() override;
};
} // namespace rive
#endif
