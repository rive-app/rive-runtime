#include "rive/generated/custom_property_boolean_base.hpp"
#include "rive/custom_property_boolean.hpp"

using namespace rive;

Core* CustomPropertyBooleanBase::clone() const
{
    auto cloned = new CustomPropertyBoolean();
    cloned->copy(*this);
    return cloned;
}
