#include "rive/generated/viewmodel/viewmodel_property_enum_custom_base.hpp"
#include "rive/viewmodel/viewmodel_property_enum_custom.hpp"

using namespace rive;

Core* ViewModelPropertyEnumCustomBase::clone() const
{
    auto cloned = new ViewModelPropertyEnumCustom();
    cloned->copy(*this);
    return cloned;
}
