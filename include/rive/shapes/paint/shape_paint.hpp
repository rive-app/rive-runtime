#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "rive/generated/shapes/paint/shape_paint_base.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint_mutator.hpp"
#include "rive/shapes/path_flags.hpp"
#include "rive/shapes/shape_paint_path.hpp"
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

public:
    StatusCode onAddedClean(CoreContext* context) override;

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

    /// Apply this ShapePaint to an external RenderPaint and optionally modulate
    /// the opacity by opacityModifer.
    virtual void applyTo(RenderPaint* renderPaint,
                         float opacityModifier) const = 0;

    void feather(Feather* feather);
    Feather* feather() const;

    virtual ShapePaintPath* pickPath(ShapePaintContainer* shape) const = 0;

private:
    Feather* m_feather = nullptr;
};
} // namespace rive

#endif
