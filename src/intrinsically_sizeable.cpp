#include "rive/intrinsically_sizeable.hpp"
#include "rive/joystick.hpp"
#include "rive/transform_component.hpp"

using namespace rive;

IntrinsicallySizeable* IntrinsicallySizeable::from(Component* component)
{
    switch (component->coreType())
    {
        case TransformComponent::typeKey:
            return component->as<TransformComponent>();
        case Joystick::typeKey:
            return component->as<Joystick>();
    }
    return nullptr;
}