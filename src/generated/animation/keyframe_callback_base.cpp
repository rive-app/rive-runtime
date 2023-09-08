#include "rive/generated/animation/keyframe_callback_base.hpp"
#include "rive/animation/keyframe_callback.hpp"

using namespace rive;

Core* KeyFrameCallbackBase::clone() const
{
    auto cloned = new KeyFrameCallback();
    cloned->copy(*this);
    return cloned;
}
