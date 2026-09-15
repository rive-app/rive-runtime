#include "rive/animation/keyframe_string.hpp"
#include "rive/generated/core_registry.hpp"
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

void KeyFrameString::apply(Core* object,
                           int propertyKey,
                           float mix,
                           const LinearAnimationInstance* context)
{
    CoreRegistry::setString(object, propertyKey, effectiveValue(context));
}

void KeyFrameString::applyInterpolation(Core* object,
                                        int propertyKey,
                                        float currentTime,
                                        const KeyFrame* nextFrame,
                                        float mix,
                                        const LinearAnimationInstance* context)
{
    CoreRegistry::setString(object, propertyKey, effectiveValue(context));
}