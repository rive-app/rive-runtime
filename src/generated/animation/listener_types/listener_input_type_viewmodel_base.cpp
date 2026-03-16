#include "rive/generated/animation/listener_types/listener_input_type_viewmodel_base.hpp"
#include "rive/animation/listener_types/listener_input_type_viewmodel.hpp"

using namespace rive;

Core* ListenerInputTypeViewModelBase::clone() const
{
    auto cloned = new ListenerInputTypeViewModel();
    cloned->copy(*this);
    return cloned;
}
