#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "rive/generated/animation/state_machine_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
class StateMachineLayer;
class StateMachineInput;
class StateMachineListener;
class StateMachineImporter;
class DataBind;
class StateMachine : public StateMachineBase
{
    friend class StateMachineImporter;

private:
    std::vector<std::unique_ptr<StateMachineLayer>> m_Layers;
    std::vector<std::unique_ptr<StateMachineInput>> m_Inputs;
    std::vector<std::unique_ptr<StateMachineListener>> m_Listeners;
    std::vector<std::unique_ptr<DataBind>> m_dataBinds;

    void addLayer(std::unique_ptr<StateMachineLayer>);
    void addInput(std::unique_ptr<StateMachineInput>);
    void addListener(std::unique_ptr<StateMachineListener>);
    void addDataBind(std::unique_ptr<DataBind>);

public:
    StateMachine();
    ~StateMachine() override;

    StatusCode import(ImportStack& importStack) override;

    size_t layerCount() const { return m_Layers.size(); }
    size_t inputCount() const { return m_Inputs.size(); }
    size_t listenerCount() const { return m_Listeners.size(); }
    size_t dataBindCount() const { return m_dataBinds.size(); }

    const StateMachineInput* input(std::string name) const;
    const StateMachineInput* input(size_t index) const;
    const StateMachineLayer* layer(std::string name) const;
    const StateMachineLayer* layer(size_t index) const;
    const DataBind* dataBind(size_t index) const;
    const StateMachineListener* listener(size_t index) const;

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
};
} // namespace rive

#endif