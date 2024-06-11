#ifndef _RIVE_ABSOLUTE_LAYOUT_COMPONENT_BASE_HPP_
#define _RIVE_ABSOLUTE_LAYOUT_COMPONENT_BASE_HPP_
#include "rive/layout_component.hpp"
namespace rive
{
class AbsoluteLayoutComponentBase : public LayoutComponent
{
protected:
    typedef LayoutComponent Super;

public:
    static const uint16_t typeKey = 423;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AbsoluteLayoutComponentBase::typeKey:
            case LayoutComponentBase::typeKey:
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