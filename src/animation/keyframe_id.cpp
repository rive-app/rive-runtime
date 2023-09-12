#include "rive/animation/keyframe_id.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

void KeyFrameId::apply(Core* object, int propertyKey, float mix)
{
    if (mix > 0) {
        CoreRegistry::setUint(object, propertyKey, value());
    }
}

void KeyFrameId::applyInterpolation(Core* object,
                                    int propertyKey,
                                    float currentTime,
                                    const KeyFrame* nextFrame,
                                    float mix)
{
    if (mix > 0) {
        CoreRegistry::setUint(object, propertyKey, value());
    }
}