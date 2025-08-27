#include "rive/generated/animation/keyframe_uint_base.hpp"
#include "rive/animation/keyframe_uint.hpp"

using namespace rive;

Core* KeyFrameUintBase::clone() const
{
    auto cloned = new KeyFrameUint();
    cloned->copy(*this);
    return cloned;
}
