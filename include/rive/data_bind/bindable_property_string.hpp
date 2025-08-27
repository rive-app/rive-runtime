#ifndef _RIVE_BINDABLE_PROPERTY_STRING_HPP_
#define _RIVE_BINDABLE_PROPERTY_STRING_HPP_
#include "rive/generated/data_bind/bindable_property_string_base.hpp"
#include <stdio.h>
#include <string_view>
namespace rive
{
class BindablePropertyString : public BindablePropertyStringBase
{
public:
    static constexpr const char* defaultValue = "";
};
} // namespace rive

#endif