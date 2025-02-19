#ifndef _RIVE_BINDABLE_PROPERTY_TRIGGER_HPP_
#define _RIVE_BINDABLE_PROPERTY_TRIGGER_HPP_
#include "rive/generated/data_bind/bindable_property_trigger_base.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyTrigger : public BindablePropertyTriggerBase
{
public:
    constexpr static uint32_t defaultValue = 0;
};
} // namespace rive

#endif