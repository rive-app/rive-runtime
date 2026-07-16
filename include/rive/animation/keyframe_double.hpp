#ifndef _RIVE_KEY_FRAME_DOUBLE_HPP_
#define _RIVE_KEY_FRAME_DOUBLE_HPP_
#include "rive/generated/animation/keyframe_double_base.hpp"
namespace rive
{
class KeyFrameDouble : public KeyFrameDoubleBase
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

    // Returns the data-bound value for this keyframe in the given instance
    // context if one exists, otherwise the authored value().
    float effectiveValue(const LinearAnimationInstance* context) const;
};
} // namespace rive

#endif