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
    void applyTo(RenderPaint* renderPaint, float opacityModifier) override;
    ShapePaintPath* pickPath(ShapePaintContainer* shape) const override;
    ShapePaintType paintType() const override { return ShapePaintType::fill; }
    void buildDependencies() override;
};
} // namespace rive

#endif