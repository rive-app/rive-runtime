#include "rive/animation/hittable.hpp"
#include "rive/component.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text_value_run.hpp"

using namespace rive;

Hittable* Hittable::from(Component* component)
{
    switch (component->coreType())
    {
        case Shape::typeKey:
            return component->as<Shape>();
        case TextValueRun::typeKey:
            return component->as<TextValueRun>();
    }
    return nullptr;
}
