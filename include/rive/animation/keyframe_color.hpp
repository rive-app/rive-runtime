#ifndef _RIVE_KEY_FRAME_COLOR_HPP_
#define _RIVE_KEY_FRAME_COLOR_HPP_
#include "rive/generated/animation/keyframe_color_base.hpp"
namespace rive
{
class KeyFrameColor : public KeyFrameColorBase
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