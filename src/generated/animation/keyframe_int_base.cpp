#include "rive/generated/animation/keyframe_int_base.hpp"
#include "rive/animation/keyframe_int.hpp"

using namespace rive;

Core* KeyFrameIntBase::clone() const
{
    auto cloned = new KeyFrameInt();
    cloned->copy(*this);
    return cloned;
}
