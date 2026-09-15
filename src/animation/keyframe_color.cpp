#include "rive/animation/keyframe_color.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/data_bind/bindable_property_color.hpp"

using namespace rive;

static void applyColor(Core* object, int propertyKey, float mix, int value)
{
    if (mix == 1.0f)
    {
        CoreRegistry::setColor(object, propertyKey, value);
    }
    else
    {
        auto mixedColor =
            colorLerp(CoreRegistry::getColor(object, propertyKey), value, mix);
        CoreRegistry::setColor(object, propertyKey, mixedColor);
    }
}

int KeyFrameColor::effectiveValue(const LinearAnimationInstance* context) const
{
    if (context != nullptr)
    {
        if (auto* holder = context->keyFrameValueHolder(this))
        {
            return holder->as<BindablePropertyColor>()->propertyValue();
        }
    }
    return value();
}

void KeyFrameColor::apply(Core* object,
                          int propertyKey,
                          float mix,
                          const LinearAnimationInstance* context)
{
    applyColor(object, propertyKey, mix, effectiveValue(context));
}

void KeyFrameColor::applyInterpolation(Core* object,
                                       int propertyKey,
                                       float currentTime,
                                       const KeyFrame* nextFrame,
                                       float mix,
                                       const LinearAnimationInstance* context)
{
    auto kfc = nextFrame->as<KeyFrameColor>();
    const KeyFrameColor& nextColor = *kfc;
    int fromValue = effectiveValue(context);
    int toValue = nextColor.effectiveValue(context);
    float f = (currentTime - seconds()) / (nextColor.seconds() - seconds());

    if (KeyFrameInterpolator* keyframeInterpolator =
            effectiveInterpolator(context))
    {
        f = keyframeInterpolator->transform(f);
    }

    applyColor(object, propertyKey, mix, colorLerp(fromValue, toValue, f));
}