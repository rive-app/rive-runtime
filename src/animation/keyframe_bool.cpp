#include "rive/animation/keyframe_bool.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

void KeyFrameBool::apply(Core* object, int propertyKey, float mix)
{
    CoreRegistry::setBool(object, propertyKey, value());
}

void KeyFrameBool::applyInterpolation(Core* object,
                                      int propertyKey,
                                      float currentTime,
                                      const KeyFrame* nextFrame,
                                      float mix,
                                      const LinearAnimationInstance* context)
{
    (void)context;
    CoreRegistry::setBool(object, propertyKey, value());
}