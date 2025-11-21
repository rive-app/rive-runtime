#ifndef _RIVE_STROKE_HPP_
#define _RIVE_STROKE_HPP_
#include "rive/generated/shapes/paint/stroke_base.hpp"
#include "rive/shapes/path_flags.hpp"
namespace rive
{
class StrokeEffect;
class Stroke : public StrokeBase
{
private:
public:
    RenderPaint* initRenderPaint(ShapePaintMutator* mutator) override;
    PathFlags pathFlags() const override;
    bool isVisible() const override;
    void applyTo(RenderPaint* renderPaint, float opacityModifier) override;
    ShapePaintPath* pickPath(ShapePaintContainer* shape) const override;

    void buildDependencies() override;
    void invalidateRendering() override;

protected:
    void thicknessChanged() override;
    void capChanged() override;
    void joinChanged() override;
    ShapePaintType paintType() override { return ShapePaintType::stroke; }
};
} // namespace rive

#endif