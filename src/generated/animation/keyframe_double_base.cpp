#include "rive/generated/animation/keyframe_double_base.hpp"
#include "rive/animation/keyframe_double.hpp"

using namespace rive;

Core* KeyFrameDoubleBase::clone() const
{
    auto cloned = new KeyFrameDouble();
    cloned->copy(*this);
    return cloned;
}
