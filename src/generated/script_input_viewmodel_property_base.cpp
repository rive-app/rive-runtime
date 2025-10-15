#include "rive/generated/script_input_viewmodel_property_base.hpp"
#include "rive/script_input_viewmodel_property.hpp"

using namespace rive;

Core* ScriptInputViewModelPropertyBase::clone() const
{
    auto cloned = new ScriptInputViewModelProperty();
    cloned->copy(*this);
    return cloned;
}
