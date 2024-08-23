#ifndef _RIVE_EVENT_BASE_HPP_
#define _RIVE_EVENT_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_callback_type.hpp"
namespace rive
{
class EventBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 128;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case EventBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t triggerPropertyKey = 395;

public:
    virtual void trigger(const CallbackData& value) = 0;

    Core* clone() const override;

protected:
};
} // namespace rive

#endif