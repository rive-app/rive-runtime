#ifndef _RIVE_NESTED_ARTBOARD_LAYOUT_BASE_HPP_
#define _RIVE_NESTED_ARTBOARD_LAYOUT_BASE_HPP_
#include "rive/nested_artboard.hpp"
namespace rive
{
class NestedArtboardLayoutBase : public NestedArtboard
{
protected:
    typedef NestedArtboard Super;

public:
    static const uint16_t typeKey = 452;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedArtboardLayoutBase::typeKey:
            case NestedArtboardBase::typeKey:
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