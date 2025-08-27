#include "rive/generated/data_bind/bindable_property_list_base.hpp"
#include "rive/data_bind/bindable_property_list.hpp"

using namespace rive;

Core* BindablePropertyListBase::clone() const
{
    auto cloned = new BindablePropertyList();
    cloned->copy(*this);
    return cloned;
}
