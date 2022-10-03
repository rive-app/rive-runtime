#include "rive/generated/animation/entry_state_base.hpp"
#include "rive/animation/entry_state.hpp"

using namespace rive;

Core* EntryStateBase::clone() const
{
    auto cloned = new EntryState();
    cloned->copy(*this);
    return cloned;
}
