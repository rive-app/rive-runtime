#ifndef _RIVE_SHAPE_PAINT_MUTATOR_HPP_
#define _RIVE_SHAPE_PAINT_MUTATOR_HPP_

#include "rive/status_code.hpp"
#include "rive/enum_bitset.hpp"

namespace rive
{
class Component;
class RenderPaint;

class ShapePaintMutator
{
protected:
    /// Hook up this paint mutator as the mutator for the shape paint
    /// expected to be the parent.
    StatusCode initPaintMutator(Component* component);
    virtual void renderOpacityChanged() = 0;

    RenderPaint* renderPaint() const { return m_renderPaint; }

public:
    enum class Flags : uint8_t
    {
        none = 0,
        visible = 1 << 0,
        translucent = 1 << 1
    };

    virtual ~ShapePaintMutator() {}

    float renderOpacity() const { return m_renderOpacity; }
    void renderOpacity(float value);

    Component* component() const { return m_component; }

    // Does this object have any alpha less than 0?
    bool isTranslucent() const;

    // Is this object visible at all (effective opacity greater than 0).
    bool isVisible() const;

    virtual void applyTo(RenderPaint* renderPaint, float opacityModifier) = 0;

protected:
    Flags m_flags;

private:
    /// The inherited opacity to modulate our color value by.
    float m_renderOpacity = 1.0f;
    RenderPaint* m_renderPaint = nullptr;
    /// The Component providing this ShapePaintMutator interface.
    Component* m_component = nullptr;
};
RIVE_MAKE_ENUM_BITSET(ShapePaintMutator::Flags);
} // namespace rive
#endif
