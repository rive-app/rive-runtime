#include "rive/importers/listener_input_type_keyboard_importer.hpp"

using namespace rive;

ListenerInputTypeKeyboardImporter::ListenerInputTypeKeyboardImporter(
    ListenerInputTypeKeyboard* listenerInputTypeKeyboard) :
    m_listenerInputTypeKeyboard(listenerInputTypeKeyboard)
{}

StatusCode ListenerInputTypeKeyboardImporter::resolve()
{
    return StatusCode::Ok;
}
