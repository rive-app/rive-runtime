#ifndef _RIVE_EVENT_BASE_HPP_
#define _RIVE_EVENT_BASE_HPP_
#include "rive/core/field_types/core_callback_type.hpp"
#include "rive/custom_property_group.hpp"
namespace rive
{
class EventBase : public CustomPropertyGroup
{
protected:
    typedef CustomPropertyGroup Super;

public:
    static const uint16_t typeKey = 128;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t triggerPropertyKey = 395;

public:
    virtual void trigger(const CallbackData& value) = 0;

    Core* clone() const override;
};
} // namespace rive

#endif