#include "rive/event.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"

using namespace rive;

void Event::trigger(const CallbackData& value)
{
    value.context()->reportEvent(this, value.delaySeconds());
}