#ifndef _RIVE_STATE_MACHINE_LAYER_COMPONENT_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_LAYER_COMPONENT_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class StateMachineLayerComponent;
class StateMachineFireEvent;

class StateMachineLayerComponentImporter : public ImportStackObject
{
public:
    StateMachineLayerComponentImporter(StateMachineLayerComponent* component);

    void addFireEvent(StateMachineFireEvent* fireEvent);

private:
    StateMachineLayerComponent* m_stateMachineLayerComponent;
};
} // namespace rive
#endif
