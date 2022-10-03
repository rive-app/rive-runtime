#ifndef _RIVE_KEY_FRAME_DOUBLE_HPP_
#define _RIVE_KEY_FRAME_DOUBLE_HPP_
#include "rive/generated/animation/keyframe_double_base.hpp"
namespace rive
{
class KeyFrameDouble : public KeyFrameDoubleBase
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