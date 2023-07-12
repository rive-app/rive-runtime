#ifndef _RIVE_RADIAL_GRADIENT_HPP_
#define _RIVE_RADIAL_GRADIENT_HPP_
#include "rive/generated/shapes/paint/radial_gradient_base.hpp"
namespace rive
{
class RadialGradient : public RadialGradientBase
{
public:
    void makeGradient(RenderPaint* renderPaint,
                      Vec2D start,
                      Vec2D end,
                      const ColorInt[],
                      const float[],
                      size_t count) const override;
};
} // namespace rive

#endif
