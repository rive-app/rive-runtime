#include "rive/generated/viewmodel/viewmodel_instance_enum_base.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"

using namespace rive;

Core* ViewModelInstanceEnumBase::clone() const
{
    auto cloned = new ViewModelInstanceEnum();
    cloned->copy(*this);
    return cloned;
}
