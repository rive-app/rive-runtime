#ifndef _RIVE_BINDABLE_PROPERTY_ARTBOARD_HPP_
#define _RIVE_BINDABLE_PROPERTY_ARTBOARD_HPP_
#include "rive/generated/data_bind/bindable_property_artboard_base.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyArtboard : public BindablePropertyArtboardBase
{
public:
    constexpr static uint32_t defaultValue = -1;
};
} // namespace rive

#endif