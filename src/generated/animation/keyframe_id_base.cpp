#include "rive/generated/animation/keyframe_id_base.hpp"
#include "rive/animation/keyframe_id.hpp"

using namespace rive;

Core* KeyFrameIdBase::clone() const
{
    auto cloned = new KeyFrameId();
    cloned->copy(*this);
    return cloned;
}
