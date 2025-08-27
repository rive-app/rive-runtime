#ifndef _RIVE_CUSTOM_PROPERTY_GROUP_BASE_HPP_
#define _RIVE_CUSTOM_PROPERTY_GROUP_BASE_HPP_
#include "rive/container_component.hpp"
namespace rive
{
class CustomPropertyGroupBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 548;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CustomPropertyGroupBase::typeKey:
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