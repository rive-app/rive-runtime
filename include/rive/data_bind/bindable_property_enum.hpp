#ifndef _RIVE_BINDABLE_PROPERTY_ENUM_HPP_
#define _RIVE_BINDABLE_PROPERTY_ENUM_HPP_
#include "rive/generated/data_bind/bindable_property_enum_base.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyEnum : public BindablePropertyEnumBase
{
public:
    constexpr static uint16_t defaultValue = 0;
};
} // namespace rive

#endif