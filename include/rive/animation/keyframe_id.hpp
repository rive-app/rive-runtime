#ifndef _RIVE_KEY_FRAME_ID_HPP_
#define _RIVE_KEY_FRAME_ID_HPP_
#include "rive/generated/animation/keyframe_id_base.hpp"

namespace rive
{
class KeyFrameId : public KeyFrameIdBase
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