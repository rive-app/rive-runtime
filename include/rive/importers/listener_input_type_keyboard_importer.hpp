#ifndef _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_IMPORTER_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_KEYBOARD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ListenerInputTypeKeyboard;

class ListenerInputTypeKeyboardImporter : public ImportStackObject
{
private:
    ListenerInputTypeKeyboard* m_listenerInputTypeKeyboard;

public:
    explicit ListenerInputTypeKeyboardImporter(
        ListenerInputTypeKeyboard* listenerInputTypeKeyboard);
    ListenerInputTypeKeyboard* listenerInputTypeKeyboard() const
    {
        return m_listenerInputTypeKeyboard;
    }
    StatusCode resolve() override;
};
} // namespace rive

#endif
