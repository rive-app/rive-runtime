#include "rive/generated/viewmodel/viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel.hpp"

using namespace rive;

Core* ViewModelBase::clone() const
{
    auto cloned = new ViewModel();
    cloned->copy(*this);
    return cloned;
}
