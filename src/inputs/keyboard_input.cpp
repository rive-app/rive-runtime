#include "rive/inputs/keyboard_input.hpp"
#include "rive/animation/listener_types/listener_input_type_keyboard.hpp"
#include "rive/generated/animation/listener_types/listener_input_type_keyboard_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/listener_input_type_keyboard_importer.hpp"

using namespace rive;

StatusCode KeyboardInput::import(ImportStack& importStack)
{
    auto* litImporter = importStack.latest<ListenerInputTypeKeyboardImporter>(
        ListenerInputTypeKeyboardBase::typeKey);
    if (litImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    litImporter->listenerInputTypeKeyboard()->addKeyboardInput(this);

    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}
