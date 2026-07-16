#include "rive/animation/keyframe_double.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/data_bind/bindable_property_number.hpp"

using namespace rive;

// This whole class is intentionally misnamed to match our editor code. The
// editor uses doubles (float64) for numeric values but at runtime 32 bit
// floating point numbers suffice. So even though this is a "double keyframe" to
// match editor names, the actual values are stored and applied in 32 bits.

static void applyDouble(Core* object, int propertyKey, float mix, float value)
{
    if (mix == 1.0f)
    {
        CoreRegistry::setDouble(object, propertyKey, value);
    }
    else
    {
        float mixi = 1.0f - mix;
        CoreRegistry::setDouble(
            object,
            propertyKey,
            CoreRegistry::getDouble(object, propertyKey) * mixi + value * mix);
    }
}

float KeyFrameDouble::effectiveValue(
    const LinearAnimationInstance* context) const
{
    if (context != nullptr)
    {
        if (auto* holder = context->keyFrameValueHolder(this))
        {
            return holder->as<BindablePropertyNumber>()->propertyValue();
        }
    }
    return value();
}

void KeyFrameDouble::apply(Core* object,
                           int propertyKey,
                           float mix,
                           const LinearAnimationInstance* context)
{
    applyDouble(object, propertyKey, mix, effectiveValue(context));
}

void KeyFrameDouble::applyInterpolation(Core* object,
                                        int propertyKey,
                                        float currentTime,
                                        const KeyFrame* nextFrame,
                                        float mix,
                                        const LinearAnimationInstance* context)
{
    auto kfd = nextFrame->as<KeyFrameDouble>();
    const KeyFrameDouble& nextDouble = *kfd;
    float fromValue = effectiveValue(context);
    float toValue = nextDouble.effectiveValue(context);
    float f = (currentTime - seconds()) / (nextDouble.seconds() - seconds());

    float frameValue;
    if (KeyFrameInterpolator* keyframeInterpolator =
            effectiveInterpolator(context))
    {
        frameValue =
            keyframeInterpolator->transformValue(fromValue, toValue, f);
    }
    else
    {
        frameValue = fromValue + (toValue - fromValue) * f;
    }

    applyDouble(object, propertyKey, mix, frameValue);
}