#include "rive/generated/animation/state_machine_bool_base.hpp"
#include "rive/animation/state_machine_bool.hpp"

using namespace rive;

Core* StateMachineBoolBase::clone() const
{
    auto cloned = new StateMachineBool();
    cloned->copy(*this);
    return cloned;
}
