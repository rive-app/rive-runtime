#ifndef _RIVE_BINDABLE_PROPERTY_NUMBER_HPP_
#define _RIVE_BINDABLE_PROPERTY_NUMBER_HPP_
#include "rive/generated/data_bind/bindable_property_number_base.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyNumber : public BindablePropertyNumberBase
{
public:
    constexpr static float defaultValue = 0;
};
} // namespace rive

#endif