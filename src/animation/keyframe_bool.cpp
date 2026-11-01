#include "rive/animation/keyframe_bool.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"

using namespace rive;

bool KeyFrameBool::effectiveValue(const LinearAnimationInstance* context) const
{
  if (context != nullptr)
  {
    if (auto* holder = context->keyFrameValueHolder(this))
    {
      return holder->as<BindablePropertyBoolean>()->propertyValue();
    }
  }
  return value();
}

static void applyBool(
    Core* object,
    int propertyKey,
    bool value,
    const LinearAnimationInstance* context)
{
  if (context != nullptr && context->blendAccumulator() != nullptr &&
      context->blendAccumulator()->applyDiscrete(object, propertyKey, value))
  {
    return;
  }
  CoreRegistry::setBool(object, propertyKey, value);
}

void KeyFrameBool::apply(
    Core* object,
    int propertyKey,
    float mix,
    const LinearAnimationInstance* context)
{
  applyBool(object, propertyKey, effectiveValue(context), context);
}

void KeyFrameBool::applyInterpolation(
    Core* object,
    int propertyKey,
    float currentTime,
    const KeyFrame* nextFrame,
    float mix,
    const LinearAnimationInstance* context)
{
  applyBool(object, propertyKey, effectiveValue(context), context);
}