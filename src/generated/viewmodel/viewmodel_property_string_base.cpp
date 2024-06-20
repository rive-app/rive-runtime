#include "rive/generated/viewmodel/viewmodel_property_string_base.hpp"
#include "rive/viewmodel/viewmodel_property_string.hpp"

using namespace rive;

Core* ViewModelPropertyStringBase::clone() const
{
    auto cloned = new ViewModelPropertyString();
    cloned->copy(*this);
    return cloned;
}
