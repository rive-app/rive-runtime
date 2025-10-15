#ifndef _RIVE_CUSTOM_PROPERTY_GROUP_HPP_
#define _RIVE_CUSTOM_PROPERTY_GROUP_HPP_
#include "rive/custom_property_container.hpp"
#include "rive/generated/custom_property_group_base.hpp"
#include <stdio.h>
namespace rive
{
class CustomPropertyGroup : public CustomPropertyGroupBase,
                            public CustomPropertyContainer
{
public:
};
} // namespace rive

#endif