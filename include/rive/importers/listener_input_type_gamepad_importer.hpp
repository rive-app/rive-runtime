#ifndef _RIVE_LISTENER_INPUT_TYPE_GAMEPAD_IMPORTER_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_GAMEPAD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ListenerInputTypeGamepad;

class ListenerInputTypeGamepadImporter : public ImportStackObject
{
private:
    ListenerInputTypeGamepad* m_listenerInputTypeGamepad;

public:
    explicit ListenerInputTypeGamepadImporter(
        ListenerInputTypeGamepad* listenerInputTypeGamepad);
    ListenerInputTypeGamepad* listenerInputTypeGamepad() const
    {
        return m_listenerInputTypeGamepad;
    }
    StatusCode resolve() override;
};
} // namespace rive

#endif
