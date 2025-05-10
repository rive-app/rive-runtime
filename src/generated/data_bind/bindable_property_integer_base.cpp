#include "rive/generated/data_bind/bindable_property_integer_base.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"

using namespace rive;

Core* BindablePropertyIntegerBase::clone() const
{
    auto cloned = new BindablePropertyInteger();
    cloned->copy(*this);
    return cloned;
}
