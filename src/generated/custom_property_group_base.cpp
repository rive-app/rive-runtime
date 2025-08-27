#include "rive/generated/custom_property_group_base.hpp"
#include "rive/custom_property_group.hpp"

using namespace rive;

Core* CustomPropertyGroupBase::clone() const
{
    auto cloned = new CustomPropertyGroup();
    cloned->copy(*this);
    return cloned;
}
