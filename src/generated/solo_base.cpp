#include "rive/generated/solo_base.hpp"
#include "rive/solo.hpp"

using namespace rive;

Core* SoloBase::clone() const
{
    auto cloned = new Solo();
    cloned->copy(*this);
    return cloned;
}
