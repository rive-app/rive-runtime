#include "rive/generated/viewmodel/viewmodel_property_enum_base.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"

using namespace rive;

Core* ViewModelPropertyEnumBase::clone() const
{
    auto cloned = new ViewModelPropertyEnum();
    cloned->copy(*this);
    return cloned;
}
