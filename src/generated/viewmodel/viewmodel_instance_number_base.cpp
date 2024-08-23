#include "rive/generated/viewmodel/viewmodel_instance_number_base.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"

using namespace rive;

Core* ViewModelInstanceNumberBase::clone() const
{
    auto cloned = new ViewModelInstanceNumber();
    cloned->copy(*this);
    return cloned;
}
