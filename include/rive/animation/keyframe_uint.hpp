#ifndef _RIVE_KEY_FRAME_UINT_HPP_
#define _RIVE_KEY_FRAME_UINT_HPP_
#include "rive/generated/animation/keyframe_uint_base.hpp"
#include <stdio.h>
namespace rive
{
class KeyFrameUint : public KeyFrameUintBase
{
public:
    void apply(Core* object, int propertyKey, float mix) override;
    void applyInterpolation(Core* object,
                            int propertyKey,
                            float seconds,
                            const KeyFrame* nextFrame,
                            float mix) override;
};
} // namespace rive

#endif