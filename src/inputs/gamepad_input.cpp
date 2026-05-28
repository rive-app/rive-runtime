#include "rive/inputs/gamepad_input.hpp"
#include "rive/animation/listener_types/listener_input_type_gamepad.hpp"
#include "rive/generated/animation/listener_types/listener_input_type_gamepad_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/listener_input_type_gamepad_importer.hpp"

using namespace rive;

StatusCode GamepadInput::import(ImportStack& importStack)
{
    auto* litImporter = importStack.latest<ListenerInputTypeGamepadImporter>(
        ListenerInputTypeGamepadBase::typeKey);
    if (litImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    litImporter->listenerInputTypeGamepad()->addGamepadInput(this);

    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}
