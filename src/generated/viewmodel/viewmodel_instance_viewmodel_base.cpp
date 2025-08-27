#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

Core* ViewModelInstanceViewModelBase::clone() const
{
    auto cloned = new ViewModelInstanceViewModel();
    cloned->copy(*this);
    return cloned;
}
