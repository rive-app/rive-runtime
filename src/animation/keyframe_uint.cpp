#include "rive/animation/keyframe_uint.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

void KeyFrameUint::apply(Core* object, int propertyKey, float mix)
{
    CoreRegistry::setUint(object, propertyKey, value());
}

void KeyFrameUint::applyInterpolation(Core* object,
                                      int propertyKey,
                                      float currentTime,
                                      const KeyFrame* nextFrame,
                                      float mix)
{
    CoreRegistry::setUint(object, propertyKey, value());
}