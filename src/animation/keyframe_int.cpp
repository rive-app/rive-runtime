#include "rive/animation/keyframe_int.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

void KeyFrameInt::apply(Core* object,
                        int propertyKey,
                        float mix,
                        const LinearAnimationInstance* context)
{
    (void)context;
    CoreRegistry::setInt(object, propertyKey, value());
}

void KeyFrameInt::applyInterpolation(Core* object,
                                     int propertyKey,
                                     float currentTime,
                                     const KeyFrame* nextFrame,
                                     float mix,
                                     const LinearAnimationInstance* context)
{
    (void)context;
    CoreRegistry::setInt(object, propertyKey, value());
}
