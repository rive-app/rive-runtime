#ifndef _RIVE_KEY_FRAME_BOOL_HPP_
#define _RIVE_KEY_FRAME_BOOL_HPP_
#include "rive/generated/animation/keyframe_bool_base.hpp"

namespace rive
{
class KeyFrameBool : public KeyFrameBoolBase
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
    bool effectiveValue(const LinearAnimationInstance* context) const;
};
} // namespace rive

#endif