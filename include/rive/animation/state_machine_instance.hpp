#ifndef _RIVE_STATE_MACHINE_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INSTANCE_HPP_

#include <string>
#include <stddef.h>
#include "rive/animation/linear_animation_instance.hpp"

namespace rive {
    class StateMachine;
    class LayerState;
    class SMIInput;
    class ArtboardInstance;
    class SMIBool;
    class SMINumber;
    class SMITrigger;

    class StateMachineLayerInstance;

    class StateMachineInstance {
        friend class SMIInput;

    private:
        const StateMachine* m_Machine;
        ArtboardInstance* m_ArtboardInstance;
        bool m_NeedsAdvance = false;

        size_t m_InputCount;
        SMIInput** m_InputInstances;
        size_t m_LayerCount;
        StateMachineLayerInstance* m_Layers;

        void markNeedsAdvance();

    public:
        StateMachineInstance(const StateMachine* machine, ArtboardInstance* instance);
        ~StateMachineInstance();

        // Advance the state machine by the specified time. Returns true if the
        // state machine will continue to animate after this advance.
        bool advance(float seconds);

        // Returns true when the StateMachineInstance has more data to process.
        bool needsAdvance() const;

        // Returns a pointer to the instance's stateMachine
        const StateMachine* stateMachine() const { return m_Machine; }

        size_t inputCount() const { return m_InputCount; }
        SMIInput* input(size_t index) const;

        SMIBool* getBool(std::string name) const;
        SMINumber* getNumber(std::string name) const;
        SMITrigger* getTrigger(std::string name) const;

        std::string name() const;

        const size_t currentAnimationCount() const;
        const LinearAnimationInstance* currentAnimationByIndex(size_t index) const;

        // The number of state changes that occurred across all layers on the
        // previous advance.
        size_t stateChangedCount() const;

        // Returns the state name for states that changed in layers on the
        // previously called advance. If the index of out of range, it returns
        // the empty string.
        const LayerState* stateChangedByIndex(size_t index) const;
    };
} // namespace rive
#endif
