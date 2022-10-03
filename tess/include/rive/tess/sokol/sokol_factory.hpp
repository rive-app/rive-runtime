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

    rcp<RenderBuffer> makeBufferU16(Span<const uint16_t>) override;
    rcp<RenderBuffer> makeBufferU32(Span<const uint32_t>) override;
    rcp<RenderBuffer> makeBufferF32(Span<const float>) override;

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
    std::unique_ptr<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    std::unique_ptr<RenderPath> makeEmptyRenderPath() override;

    std::unique_ptr<RenderPaint> makeRenderPaint() override;
};
} // namespace rive
#endif
