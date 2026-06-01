#include "rive/importers/listener_input_type_gamepad_importer.hpp"

using namespace rive;

ListenerInputTypeGamepadImporter::ListenerInputTypeGamepadImporter(
    ListenerInputTypeGamepad* listenerInputTypeGamepad) :
    m_listenerInputTypeGamepad(listenerInputTypeGamepad)
{}

StatusCode ListenerInputTypeGamepadImporter::resolve()
{
    return StatusCode::Ok;
}
