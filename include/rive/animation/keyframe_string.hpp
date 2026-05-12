#ifndef _RIVE_KEY_FRAME_STRING_HPP_
#define _RIVE_KEY_FRAME_STRING_HPP_
#include "rive/generated/animation/keyframe_string_base.hpp"

namespace rive
{
class KeyFrameString : public KeyFrameStringBase
{
public:
    void apply(Core* object, int propertyKey, float mix) override;
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