#ifndef _RIVE_CUSTOM_PROPERTY_TRIGGER_HPP_
#define _RIVE_CUSTOM_PROPERTY_TRIGGER_HPP_
#include "rive/generated/custom_property_trigger_base.hpp"
#include "rive/resetting_component.hpp"
#include <stdio.h>
namespace rive
{
class CustomPropertyTrigger : public CustomPropertyTriggerBase,
                              public ResettingComponent
{
public:
    void fire(const CallbackData& value) override
    {
        propertyValue(propertyValue() + 1);
    }

    void reset() override { propertyValue(0); }
};
} // namespace rive

#endif