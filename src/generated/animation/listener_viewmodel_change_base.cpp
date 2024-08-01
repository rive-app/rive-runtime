#include "rive/generated/animation/listener_viewmodel_change_base.hpp"
#include "rive/animation/listener_viewmodel_change.hpp"

using namespace rive;

Core* ListenerViewModelChangeBase::clone() const
{
    auto cloned = new ListenerViewModelChange();
    cloned->copy(*this);
    return cloned;
}
