#include "rive/generated/data_bind/bindable_property_number_base.hpp"
#include "rive/data_bind/bindable_property_number.hpp"

using namespace rive;

Core* BindablePropertyNumberBase::clone() const
{
    auto cloned = new BindablePropertyNumber();
    cloned->copy(*this);
    return cloned;
}
