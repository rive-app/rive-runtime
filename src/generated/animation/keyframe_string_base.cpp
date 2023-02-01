#include "rive/generated/animation/keyframe_string_base.hpp"
#include "rive/animation/keyframe_string.hpp"

using namespace rive;

Core* KeyFrameStringBase::clone() const
{
    auto cloned = new KeyFrameString();
    cloned->copy(*this);
    return cloned;
}
