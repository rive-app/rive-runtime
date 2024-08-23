#include "rive/animation/keyframe_color.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/shapes/paint/color.hpp"

using namespace rive;

static void applyColor(Core* object, int propertyKey, float mix, int value)
{
    if (mix == 1.0f)
    {
        CoreRegistry::setColor(object, propertyKey, value);
    }
    else
    {
        auto mixedColor = colorLerp(CoreRegistry::getColor(object, propertyKey), value, mix);
        CoreRegistry::setColor(object, propertyKey, mixedColor);
    }
}

void KeyFrameColor::apply(Core* object, int propertyKey, float mix)
{
    applyColor(object, propertyKey, mix, value());
}

void KeyFrameColor::applyInterpolation(Core* object,
                                       int propertyKey,
                                       float currentTime,
                                       const KeyFrame* nextFrame,
                                       float mix)
{
    auto kfc = nextFrame->as<KeyFrameColor>();
    const KeyFrameColor& nextColor = *kfc;
    float f = (currentTime - seconds()) / (nextColor.seconds() - seconds());

    if (KeyFrameInterpolator* keyframeInterpolator = interpolator())
    {
        f = keyframeInterpolator->transform(f);
    }

    applyColor(object, propertyKey, mix, colorLerp(value(), nextColor.value(), f));
}