#include "rive/generated/viewmodel/viewmodel_instance_value_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"

using namespace rive;

Core* ViewModelInstanceValueBase::clone() const
{
    auto cloned = new ViewModelInstanceValue();
    cloned->copy(*this);
    return cloned;
}
