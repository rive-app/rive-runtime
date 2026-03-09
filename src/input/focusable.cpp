#include "rive/input/focusable.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/text/text_input.hpp"

using namespace rive;

Focusable* Focusable::from(Core* object)
{
    if (object == nullptr)
    {
        return nullptr;
    }

    if (object->is<TextInput>())
    {
        return object->as<TextInput>();
    }
    if (object->is<NestedArtboard>())
    {
        return object->as<NestedArtboard>();
    }
    return nullptr;
}
