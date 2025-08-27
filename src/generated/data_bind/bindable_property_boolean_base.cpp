#include "rive/generated/data_bind/bindable_property_boolean_base.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"

using namespace rive;

Core* BindablePropertyBooleanBase::clone() const
{
    auto cloned = new BindablePropertyBoolean();
    cloned->copy(*this);
    return cloned;
}
