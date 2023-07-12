#ifndef _RIVE_STROKE_HPP_
#define _RIVE_STROKE_HPP_
#include "rive/generated/shapes/paint/stroke_base.hpp"
#include "rive/shapes/path_space.hpp"
namespace rive
{
class StrokeEffect;
class Stroke : public StrokeBase
{
private:
    StrokeEffect* m_Effect = nullptr;

public:
    RenderPaint* initRenderPaint(ShapePaintMutator* mutator) override;
    PathSpace pathSpace() const override;
    void draw(Renderer* renderer, CommandPath* path, RenderPaint* paint) override;
    void addStrokeEffect(StrokeEffect* effect);
    bool hasStrokeEffect() { return m_Effect != nullptr; }
    void invalidateEffects();
    bool isVisible() const override;
    void invalidateRendering();
    void applyTo(RenderPaint* renderPaint, float opacityModifier) const override;

protected:
    void thicknessChanged() override;
    void capChanged() override;
    void joinChanged() override;
};
} // namespace rive

#endif