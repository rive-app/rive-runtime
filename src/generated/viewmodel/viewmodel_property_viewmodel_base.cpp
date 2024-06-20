#include "rive/generated/viewmodel/viewmodel_property_viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"

using namespace rive;

Core* ViewModelPropertyViewModelBase::clone() const
{
    auto cloned = new ViewModelPropertyViewModel();
    cloned->copy(*this);
    return cloned;
}
