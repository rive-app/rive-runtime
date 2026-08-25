#include "rive/animation/keyframe_id.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

void KeyFrameId::apply(Core* object,
                       int propertyKey,
                       float mix,
                       const LinearAnimationInstance* context)
{
    (void)context;
    // setUint would narrow through `uint32_t` and drop the client half
    // of an editor-mode Id.
    CoreRegistry::setId(object, propertyKey, value());
}

void KeyFrameId::applyInterpolation(Core* object,
                                    int propertyKey,
                                    float currentTime,
                                    const KeyFrame* nextFrame,
                                    float mix,
                                    const LinearAnimationInstance* context)
{
    (void)context;
    CoreRegistry::setId(object, propertyKey, value());
}