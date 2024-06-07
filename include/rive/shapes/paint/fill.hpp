#ifndef _RIVE_FILL_HPP_
#define _RIVE_FILL_HPP_
#include "rive/generated/shapes/paint/fill_base.hpp"
#include "rive/shapes/path_flags.hpp"
namespace rive
{
class Fill : public FillBase
{
public:
    RenderPaint* initRenderPaint(ShapePaintMutator* mutator) override;
    PathFlags pathFlags() const override;
    void draw(Renderer* renderer,
              CommandPath* path,
              const RawPath* rawPath,
              RenderPaint* paint) override;
    void applyTo(RenderPaint* renderPaint, float opacityModifier) const override;
};
} // namespace rive

#endif