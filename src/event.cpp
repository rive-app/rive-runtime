#include "rive/event.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"

using namespace rive;

void Event::trigger(const CallbackData& value)
{
    value.context()->reportEvent(this, value.delaySeconds());
}

StatusCode Event::import(ImportStack& importStack)
{
    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addEvent(this);
    return Super::import(importStack);
}