#include "rive/generated/viewmodel/viewmodel_property_list_base.hpp"
#include "rive/viewmodel/viewmodel_property_list.hpp"

using namespace rive;

Core* ViewModelPropertyListBase::clone() const
{
    auto cloned = new ViewModelPropertyList();
    cloned->copy(*this);
    return cloned;
}
