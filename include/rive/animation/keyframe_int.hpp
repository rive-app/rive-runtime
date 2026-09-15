#ifndef _RIVE_KEY_FRAME_INT_HPP_
#define _RIVE_KEY_FRAME_INT_HPP_
#include "rive/generated/animation/keyframe_int_base.hpp"
#include <stdio.h>
namespace rive
{
class KeyFrameInt : public KeyFrameIntBase
{
public:
    void apply(Core* object,
               int propertyKey,
               float mix,
               const LinearAnimationInstance* context = nullptr) override;
    void applyInterpolation(
        Core* object,
        int propertyKey,
        float seconds,
        const KeyFrame* nextFrame,
        float mix,
        const LinearAnimationInstance* context = nullptr) override;
};
} // namespace rive

#endif
