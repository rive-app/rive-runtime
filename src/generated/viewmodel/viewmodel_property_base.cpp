#include "rive/generated/viewmodel/viewmodel_property_base.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"

using namespace rive;

Core* ViewModelPropertyBase::clone() const
{
    auto cloned = new ViewModelProperty();
    cloned->copy(*this);
    return cloned;
}
