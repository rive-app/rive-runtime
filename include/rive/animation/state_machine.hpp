#ifndef _RIVE_STATE_MACHINE_HPP_
#define _RIVE_STATE_MACHINE_HPP_
#include "rive/generated/animation/state_machine_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive {
    class StateMachineLayer;
    class StateMachineInput;
    class StateMachineImporter;
    class StateMachine : public StateMachineBase {
        friend class StateMachineImporter;

    private:
        std::vector<StateMachineLayer*> m_Layers;
        std::vector<StateMachineInput*> m_Inputs;

        void addLayer(StateMachineLayer* layer);
        void addInput(StateMachineInput* input);

    public:
        ~StateMachine();
        StatusCode import(ImportStack& importStack) override;

        size_t layerCount() const { return m_Layers.size(); }
        size_t inputCount() const { return m_Inputs.size(); }

        const StateMachineInput* input(std::string name) const;
        const StateMachineInput* input(size_t index) const;
        const StateMachineLayer* layer(std::string name) const;
        const StateMachineLayer* layer(size_t index) const;

        StatusCode onAddedDirty(CoreContext* context) override;
        StatusCode onAddedClean(CoreContext* context) override;
    };
} // namespace rive

#endif