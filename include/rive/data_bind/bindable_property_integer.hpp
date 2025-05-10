#ifndef _RIVE_BINDABLE_PROPERTY_INTEGER_HPP_
#define _RIVE_BINDABLE_PROPERTY_INTEGER_HPP_
#include "rive/generated/data_bind/bindable_property_integer_base.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyInteger : public BindablePropertyIntegerBase
{
public:
    constexpr static uint32_t defaultValue = 0;
};
} // namespace rive

#endif