#include "rive/animation/keyframe_id.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/linear_animation_instance.hpp"

using namespace rive;

static void applyId(
    Core* object,
    int propertyKey,
    Id value,
    const LinearAnimationInstance* context)
{
#ifndef WITH_RIVE_EDITOR
  // Runtime ids are 32 bits, which the accumulator holds whole. Editor
  // builds don't collect layer writes.
  if (context != nullptr && context->blendAccumulator() != nullptr &&
      context->blendAccumulator()->applyDiscrete(object, propertyKey, value))
  {
    return;
  }
#else
  (void)context;
#endif
  // setUint would narrow through `uint32_t` and drop the client half
  // of an editor-mode Id.
  CoreRegistry::setId(object, propertyKey, value);
}

void KeyFrameId::apply(
    Core* object,
    int propertyKey,
    float mix,
    const LinearAnimationInstance* context)
{
  applyId(object, propertyKey, value(), context);
}

void KeyFrameId::applyInterpolation(
    Core* object,
    int propertyKey,
    float currentTime,
    const KeyFrame* nextFrame,
    float mix,
    const LinearAnimationInstance* context)
{
  applyId(object, propertyKey, value(), context);
}