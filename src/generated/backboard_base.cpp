#include "rive/generated/backboard_base.hpp"
#include "rive/backboard.hpp"

using namespace rive;

Core* BackboardBase::clone() const
{
    auto cloned = new Backboard();
    cloned->copy(*this);
    return cloned;
}
