#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "rive/generated/animation/state_machine_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive {
    class StateMachineLayer;
    class StateMachineInput;
    class StateMachineEvent;
    class StateMachineImporter;
    class StateMachine : public StateMachineBase {
        friend class StateMachineImporter;

    private:
        std::vector<StateMachineLayer*> m_Layers;
        std::vector<StateMachineInput*> m_Inputs;
        std::vector<StateMachineEvent*> m_Events;

        void addLayer(StateMachineLayer* layer);
        void addInput(StateMachineInput* input);
        void addEvent(StateMachineEvent* event);

    public:
        ~StateMachine();
        StatusCode import(ImportStack& importStack) override;

        size_t layerCount() const { return m_Layers.size(); }
        size_t inputCount() const { return m_Inputs.size(); }
        size_t eventCount() const { return m_Events.size(); }

        const StateMachineInput* input(std::string name) const;
        const StateMachineInput* input(size_t index) const;
        const StateMachineLayer* layer(std::string name) const;
        const StateMachineLayer* layer(size_t index) const;
        const StateMachineEvent* event(size_t index) const;

        StatusCode onAddedDirty(CoreContext* context) override;
        StatusCode onAddedClean(CoreContext* context) override;
    };
} // namespace rive

#endif