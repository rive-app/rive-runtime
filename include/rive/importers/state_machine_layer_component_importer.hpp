#ifndef _RIVE_STATE_MACHINE_LAYER_COMPONENT_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_LAYER_COMPONENT_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <memory>

namespace rive
{
class StateMachineLayerComponent;
class StateMachineFireAction;
class ListenerAction;

class StateMachineLayerComponentImporter : public ImportStackObject
{
public:
    StateMachineLayerComponentImporter(StateMachineLayerComponent* component);

    void addFireEvent(StateMachineFireAction* fireEvent);
    void addListenerAction(std::unique_ptr<ListenerAction> action);

private:
    StateMachineLayerComponent* m_stateMachineLayerComponent;
};
} // namespace rive
#endif
