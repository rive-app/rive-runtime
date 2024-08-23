#ifndef _RIVE_KEY_FRAME_BOOL_HPP_
#define _RIVE_KEY_FRAME_BOOL_HPP_
#include "rive/generated/animation/keyframe_bool_base.hpp"

namespace rive
{
class KeyFrameBool : public KeyFrameBoolBase
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