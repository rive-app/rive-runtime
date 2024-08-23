#include "rive/animation/keyframe_double.hpp"
#include "rive/generated/core_registry.hpp"

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
        CoreRegistry::setDouble(object,
                                propertyKey,
                                CoreRegistry::getDouble(object, propertyKey) * mixi + value * mix);
    }
}

void KeyFrameDouble::apply(Core* object, int propertyKey, float mix)
{
    applyDouble(object, propertyKey, mix, value());
}

void KeyFrameDouble::applyInterpolation(Core* object,
                                        int propertyKey,
                                        float currentTime,
                                        const KeyFrame* nextFrame,
                                        float mix)
{
    auto kfd = nextFrame->as<KeyFrameDouble>();
    const KeyFrameDouble& nextDouble = *kfd;
    float f = (currentTime - seconds()) / (nextDouble.seconds() - seconds());

    float frameValue;
    if (KeyFrameInterpolator* keyframeInterpolator = interpolator())
    {
        frameValue = keyframeInterpolator->transformValue(value(), nextDouble.value(), f);
    }
    else
    {
        frameValue = value() + (nextDouble.value() - value()) * f;
    }

    applyDouble(object, propertyKey, mix, frameValue);
}