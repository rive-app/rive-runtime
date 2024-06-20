#include "rive/generated/viewmodel/viewmodel_component_base.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"

using namespace rive;

Core* ViewModelComponentBase::clone() const
{
    auto cloned = new ViewModelComponent();
    cloned->copy(*this);
    return cloned;
}
