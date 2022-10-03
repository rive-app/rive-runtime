#ifndef _RIVE_SHAPE_BASE_HPP_
#define _RIVE_SHAPE_BASE_HPP_
#include "rive/drawable.hpp"
namespace rive
{
class ShapeBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 3;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ShapeBase::typeKey:
            case DrawableBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif