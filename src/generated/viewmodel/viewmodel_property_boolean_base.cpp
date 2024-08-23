#include "rive/generated/viewmodel/viewmodel_property_boolean_base.hpp"
#include "rive/viewmodel/viewmodel_property_boolean.hpp"

using namespace rive;

Core* ViewModelPropertyBooleanBase::clone() const
{
    auto cloned = new ViewModelPropertyBoolean();
    cloned->copy(*this);
    return cloned;
}
