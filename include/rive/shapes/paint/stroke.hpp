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
    StrokeEffect* m_Effect = nullptr;

public:
    RenderPaint* initRenderPaint(ShapePaintMutator* mutator) override;
    PathFlags pathFlags() const override;
    void addStrokeEffect(StrokeEffect* effect);
    bool hasStrokeEffect() { return m_Effect != nullptr; }
    void invalidateEffects();
    bool isVisible() const override;
    void invalidateRendering();
    void applyTo(RenderPaint* renderPaint,
                 float opacityModifier) const override;
    ShapePaintPath* pickPath(ShapePaintContainer* shape) const override;

    void draw(Renderer* renderer,
              ShapePaintPath* shapePaintPath,
              const Mat2D& transform,
              bool usePathFillRule,
              RenderPaint* overridePaint) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
#ifdef TESTING
    StrokeEffect* effect() { return m_Effect; }
#endif

protected:
    void thicknessChanged() override;
    void capChanged() override;
    void joinChanged() override;
};
} // namespace rive

#endif