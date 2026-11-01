#include "rive/animation/keyframe_string.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/data_bind/bindable_property_string.hpp"

using namespace rive;

const std::string& KeyFrameString::effectiveValue(
    const LinearAnimationInstance* context) const
{
  if (context != nullptr)
  {
    if (auto* holder = context->keyFrameValueHolder(this))
    {
      return holder->as<BindablePropertyString>()->propertyValue();
    }
  }
  return value();
}

static void applyString(
    Core* object,
    int propertyKey,
    const std::string& value,
    const LinearAnimationInstance* context)
{
  if (context != nullptr && context->blendAccumulator() != nullptr &&
      context->blendAccumulator()->applyString(object, propertyKey, value))
  {
    return;
  }
  CoreRegistry::setString(object, propertyKey, value);
}

void KeyFrameString::apply(
    Core* object,
    int propertyKey,
    float mix,
    const LinearAnimationInstance* context)
{
  applyString(object, propertyKey, effectiveValue(context), context);
}

void KeyFrameString::applyInterpolation(
    Core* object,
    int propertyKey,
    float currentTime,
    const KeyFrame* nextFrame,
    float mix,
    const LinearAnimationInstance* context)
{
  applyString(object, propertyKey, effectiveValue(context), context);
}