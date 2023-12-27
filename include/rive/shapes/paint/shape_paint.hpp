#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "rive/generated/shapes/paint/shape_paint_base.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint_mutator.hpp"
#include "rive/shapes/path_space.hpp"
namespace rive
{
class RenderPaint;
class ShapePaintMutator;
class ShapePaint : public ShapePaintBase
{
protected:
    rcp<RenderPaint> m_RenderPaint;
    ShapePaintMutator* m_PaintMutator = nullptr;

public:
    StatusCode onAddedClean(CoreContext* context) override;

    float renderOpacity() const { return m_PaintMutator->renderOpacity(); }
    void renderOpacity(float value) { m_PaintMutator->renderOpacity(value); }

    void blendMode(BlendMode value);

    /// Creates a RenderPaint object for the provided ShapePaintMutator*.
    /// This should be called only once as the ShapePaint manages the
    /// lifecycle of the RenderPaint.
    virtual RenderPaint* initRenderPaint(ShapePaintMutator* mutator);

    virtual PathSpace pathSpace() const = 0;

    void draw(Renderer* renderer, CommandPath* path) { draw(renderer, path, renderPaint()); }

    virtual void draw(Renderer* renderer, CommandPath* path, RenderPaint* paint) = 0;

    RenderPaint* renderPaint() { return m_RenderPaint.get(); }

    /// Get the component that represents the ShapePaintMutator for this
    /// ShapePaint. It'll be one of SolidColor, LinearGradient, or
    /// RadialGradient.
    Component* paint() const { return m_PaintMutator->component(); }

    bool isTranslucent() const { return !this->isVisible() || m_PaintMutator->isTranslucent(); }

    /// Apply this ShapePaint to an external RenderPaint and optionally modulate
    /// the opacity by opacityModifer.
    virtual void applyTo(RenderPaint* renderPaint, float opacityModifier) const = 0;
};
} // namespace rive

#endif
