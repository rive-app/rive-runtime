#include "rive/generated/animation/exit_state_base.hpp"
#include "rive/animation/exit_state.hpp"

using namespace rive;

Core* ExitStateBase::clone() const
{
    auto cloned = new ExitState();
    cloned->copy(*this);
    return cloned;
}
