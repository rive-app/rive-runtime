#ifndef _RIVE_STATE_MACHINE_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class StateMachineInput;
class StateMachineLayer;
class StateMachineListener;
class StateMachine;
class StateMachineImporter : public ImportStackObject
{
private:
    StateMachine* m_StateMachine;

public:
    StateMachineImporter(StateMachine* machine);
    const StateMachine* stateMachine() const { return m_StateMachine; }

    void addLayer(std::unique_ptr<StateMachineLayer>);
    void addInput(std::unique_ptr<StateMachineInput>);
    void addListener(std::unique_ptr<StateMachineListener>);

    StatusCode resolve() override;
    bool readNullObject() override;
};
} // namespace rive
#endif
