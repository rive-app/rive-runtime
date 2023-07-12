#ifndef _RIVE_SHAPE_PAINT_MUTATOR_HPP_
#define _RIVE_SHAPE_PAINT_MUTATOR_HPP_

namespace rive
{
class Component;
class RenderPaint;
class ShapePaintMutator
{
private:
    float m_RenderOpacity = 1.0f;
    RenderPaint* m_RenderPaint = nullptr;
    /// The Component providing this ShapePaintMutator interface.
    Component* m_Component = nullptr;

protected:
    /// Hook up this paint mutator as the mutator for the shape paint
    /// expected to be the parent.
    bool initPaintMutator(Component* component);
    virtual void renderOpacityChanged() = 0;

    RenderPaint* renderPaint() const { return m_RenderPaint; }

    virtual bool internalIsTranslucent() const = 0;

public:
    virtual ~ShapePaintMutator() {}

    float renderOpacity() const { return m_RenderOpacity; }
    void renderOpacity(float value);

    Component* component() const { return m_Component; }

    bool isTranslucent() const { return m_RenderOpacity < 1 || this->internalIsTranslucent(); }

    virtual void applyTo(RenderPaint* renderPaint, float opacityModifier) const = 0;
};
} // namespace rive
#endif
