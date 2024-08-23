#include "rive/animation/keyframe_string.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

void KeyFrameString::apply(Core* object, int propertyKey, float mix)
{
    CoreRegistry::setString(object, propertyKey, value());
}

void KeyFrameString::applyInterpolation(Core* object,
                                        int propertyKey,
                                        float currentTime,
                                        const KeyFrame* nextFrame,
                                        float mix)
{
    CoreRegistry::setString(object, propertyKey, value());
}