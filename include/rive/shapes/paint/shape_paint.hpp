#ifndef _RIVE_SHAPE_PAINT_HPP_
#define _RIVE_SHAPE_PAINT_HPP_
#include "rive/generated/shapes/paint/shape_paint_base.hpp"
#include "rive/shapes/paint/effects_container.hpp"
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
class TransformComponent;
class ShapePaint : public ShapePaintBase,
                   public EffectsContainer,
                   public PathProvider
{
protected:
    rcp<RenderPaint> m_RenderPaint;
    ShapePaintMutator* m_PaintMutator = nullptr;

public:
    StatusCode onAddedClean(CoreContext* context) override;
    void invalidateEffects(StrokeEffect* effect) override;
    void invalidateEffects() override;
    virtual void invalidateRendering();

    float renderOpacity() const { return m_PaintMutator->renderOpacity(); }
    void renderOpacity(float value) { m_PaintMutator->renderOpacity(value); }

    void blendMode(BlendMode value);

    void addStrokeEffect(StrokeEffect* effect) override;

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
                      RenderPaint* overridePaint = nullptr,
                      bool needsSaveOperation = true);

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
#ifdef WITH_RIVE_EDITOR
    /// Set the feather pointer in editor mode (idempotent).
    void setFeatherForEditor(Feather* f) { m_feather = f; }
    /// Clear m_feather only if it currently points at `expected`.
    void clearFeatherIfForEditor(Feather* expected)
    {
        if (m_feather == expected)
        {
            m_feather = nullptr;
        }
    }
    // Edit-time reparent/remove dispatch — registers this paint into
    // the new parent's `m_ShapePaints` and removes from the old
    // parent's. Without this, removing a Fill/Stroke leaves a stale
    // pointer in the host Shape's m_ShapePaints, which `buildDeps`
    // and `pathChanged` iterate and crash on. Body in
    // `component_parent_editor.cpp`.
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif

    virtual ShapePaintPath* pickPath(ShapePaintContainer* shape) const = 0;
    void update(ComponentDirt value) override;
    virtual ShapePaintType paintType() const = 0;

    TransformComponent* parentTransformComponent() const;

private:
    Feather* m_feather = nullptr;
};
} // namespace rive

#endif
