#include "rive/generated/animation/any_state_base.hpp"
#include "rive/animation/any_state.hpp"

using namespace rive;

Core* AnyStateBase::clone() const
{
    auto cloned = new AnyState();
    cloned->copy(*this);
    return cloned;
}
