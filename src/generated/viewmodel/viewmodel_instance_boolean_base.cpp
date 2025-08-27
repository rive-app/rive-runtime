#include "rive/generated/viewmodel/viewmodel_instance_boolean_base.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"

using namespace rive;

Core* ViewModelInstanceBooleanBase::clone() const
{
    auto cloned = new ViewModelInstanceBoolean();
    cloned->copy(*this);
    return cloned;
}
