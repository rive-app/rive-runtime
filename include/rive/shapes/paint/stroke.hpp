#ifndef _RIVE_STROKE_HPP_
#define _RIVE_STROKE_HPP_
#include "rive/generated/shapes/paint/stroke_base.hpp"
#include "rive/shapes/path_flags.hpp"
#include "rive/shapes/paint/stroke_position.hpp"
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
    void update(ComponentDirt value) override;
    void invalidateRendering() override;
    ShapePaintType paintType() const override { return ShapePaintType::stroke; }
    StrokePosition strokePosition() const;

protected:
    void thicknessChanged() override;
    void capChanged() override;
    void joinChanged() override;
    void positionChanged() override;
};
} // namespace rive

#endif