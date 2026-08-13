#include "rive/animation/keyframe_uint.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/generated/core_registry.hpp"
#include <cmath>

using namespace rive;

// Uints hold by default -- most are enums, ids or mode flags. Defs opting in
// with "interpolates": true tween instead, via the generated
// CoreRegistry::isInterpolatableUint. Rounding and mixing must match
// packages/rive_core/lib/animation/keyframe_uint.dart or editor preview and
// runtime playback drift.

// Clamps negatives to zero: a cubic interpolator can overshoot below zero and
// the destination property is unsigned.
static uint32_t roundToUint(float value)
{
    return value <= 0.0f ? 0u : static_cast<uint32_t>(std::lround(value));
}

static void applyUint(Core* object, int propertyKey, float mix, uint32_t value)
{
    if (mix == 1.0f)
    {
        CoreRegistry::setUint(object, propertyKey, value);
    }
    else
    {
        float mixi = 1.0f - mix;
        CoreRegistry::setUint(
            object,
            propertyKey,
            roundToUint(CoreRegistry::getUint(object, propertyKey) * mixi +
                        value * mix));
    }
}

void KeyFrameUint::apply(Core* object,
                         int propertyKey,
                         float mix,
                         const LinearAnimationInstance* context)
{
    (void)context;
    if (CoreRegistry::isInterpolatableUint(propertyKey))
    {
        applyUint(object, propertyKey, mix, value());
        return;
    }
    CoreRegistry::setUint(object, propertyKey, value());
}

void KeyFrameUint::applyInterpolation(Core* object,
                                      int propertyKey,
                                      float currentTime,
                                      const KeyFrame* nextFrame,
                                      float mix,
                                      const LinearAnimationInstance* context)
{
    if (!CoreRegistry::isInterpolatableUint(propertyKey))
    {
        CoreRegistry::setUint(object, propertyKey, value());
        return;
    }

    const KeyFrameUint& nextUint = *nextFrame->as<KeyFrameUint>();
    float fromValue = static_cast<float>(value());
    float toValue = static_cast<float>(nextUint.value());
    float f = (currentTime - seconds()) / (nextUint.seconds() - seconds());

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

    applyUint(object, propertyKey, mix, roundToUint(frameValue));
}
