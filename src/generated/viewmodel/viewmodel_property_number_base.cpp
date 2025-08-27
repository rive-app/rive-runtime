#include "rive/generated/viewmodel/viewmodel_property_number_base.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"

using namespace rive;

Core* ViewModelPropertyNumberBase::clone() const
{
    auto cloned = new ViewModelPropertyNumber();
    cloned->copy(*this);
    return cloned;
}
