#include "rive/generated/data_bind/bindable_property_viewmodel_base.hpp"
#include "rive/data_bind/bindable_property_viewmodel.hpp"

using namespace rive;

Core* BindablePropertyViewModelBase::clone() const
{
    auto cloned = new BindablePropertyViewModel();
    cloned->copy(*this);
    return cloned;
}
