#include "rive/generated/viewmodel/viewmodel_property_enum_system_base.hpp"
#include "rive/viewmodel/viewmodel_property_enum_system.hpp"

using namespace rive;

Core* ViewModelPropertyEnumSystemBase::clone() const
{
    auto cloned = new ViewModelPropertyEnumSystem();
    cloned->copy(*this);
    return cloned;
}
