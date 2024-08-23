#include "rive/generated/viewmodel/viewmodel_instance_base.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

using namespace rive;

Core* ViewModelInstanceBase::clone() const
{
    auto cloned = new ViewModelInstance();
    cloned->copy(*this);
    return cloned;
}
