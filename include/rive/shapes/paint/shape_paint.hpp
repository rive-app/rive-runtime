#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "rive/generated/shapes/paint/shape_paint_base.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint_mutator.hpp"
#include "rive/shapes/path_flags.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/math/raw_path.hpp"

namespace rive
{
class RenderPaint;
class ShapePaintMutator;
class Feather;
class ShapePaintContainer;
class ShapePaint : public ShapePaintBase
{
protected:
    rcp<RenderPaint> m_RenderPaint;
    ShapePaintMutator* m_PaintMutator = nullptr;
    std::vector<StrokeEffect*> m_effects;
    virtual ShapePaintType paintType() = 0;

public:
    StatusCode onAddedClean(CoreContext* context) override;
    void addStrokeEffect(StrokeEffect* effect);
    bool hasStrokeEffect() { return m_effects.size() > 0; }
    void invalidateEffects(StrokeEffect* effect);
    void invalidateEffects();
    virtual void invalidateRendering();

    float renderOpacity() const { return m_PaintMutator->renderOpacity(); }
    void renderOpacity(float value) { m_PaintMutator->renderOpacity(value); }

    void blendMode(BlendMode value);

    /// Creates a RenderPaint object for the provided ShapePaintMutator*.
    /// This should be called only once as the ShapePaint manages the
    /// lifecycle of the RenderPaint.
    virtual RenderPaint* initRenderPaint(ShapePaintMutator* mutator);

    virtual PathFlags pathFlags() const = 0;
    bool isFlagged(PathFlags flags) const
    {
        return (int)(pathFlags() & flags) != 0x00;
    }

    virtual void draw(Renderer* renderer,
                      ShapePaintPath* shapePaintPath,
                      const Mat2D& transform,
                      bool usePathFillRule = false,
                      RenderPaint* overridePaint = nullptr);

    RenderPaint* renderPaint() { return m_RenderPaint.get(); }

    /// Get the component that represents the ShapePaintMutator for this
    /// ShapePaint. It'll be one of SolidColor, LinearGradient, or
    /// RadialGradient.
    Component* paint() const { return m_PaintMutator->component(); }

    bool isTranslucent() const
    {
        return !this->isVisible() || m_PaintMutator->isTranslucent();
    }

    bool shouldDraw() const
    {
        return this->isVisible() && m_PaintMutator->isVisible();
    }

    /// Apply this ShapePaint to an external RenderPaint and optionally modulate
    /// the opacity by opacityModifer.
    virtual void applyTo(RenderPaint* renderPaint, float opacityModifier) = 0;

    void feather(Feather* feather);
    Feather* feather() const;

    virtual ShapePaintPath* pickPath(ShapePaintContainer* shape) const = 0;
    void update(ComponentDirt value) override;
#ifdef TESTING
    StrokeEffect* effect()
    {
        return m_effects.size() > 0 ? m_effects.back() : nullptr;
    }
#endif

private:
    Feather* m_feather = nullptr;
};
} // namespace rive

#endif
