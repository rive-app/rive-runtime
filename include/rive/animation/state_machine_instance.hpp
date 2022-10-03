#ifndef _RIVE_STATE_MACHINE_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INSTANCE_HPP_

#include <string>
#include <stddef.h>
#include <vector>
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/listener_type.hpp"
#include "rive/scene.hpp"

namespace rive
{
class StateMachine;
class LayerState;
class SMIInput;
class ArtboardInstance;
class SMIBool;
class SMINumber;
class SMITrigger;
class Shape;
class StateMachineLayerInstance;
class HitShape;
class NestedArtboard;

class StateMachineInstance : public Scene
{
    friend class SMIInput;

private:
    const StateMachine* m_Machine;
    bool m_NeedsAdvance = false;

    std::vector<SMIInput*> m_InputInstances; // we own each pointer
    size_t m_LayerCount;
    StateMachineLayerInstance* m_Layers;

    void markNeedsAdvance();

    std::vector<std::unique_ptr<HitShape>> m_HitShapes;
    std::vector<NestedArtboard*> m_HitNestedArtboards;

    /// Provide a hitListener if you want to process a down or an up for the pointer position
    /// too.
    void updateListeners(Vec2D position, ListenerType hitListener);

    template <typename SMType, typename InstType>
    InstType* getNamedInput(const std::string& name) const;

public:
    StateMachineInstance(const StateMachine* machine, ArtboardInstance* instance);
    StateMachineInstance(StateMachineInstance const&) = delete;
    ~StateMachineInstance() override;

    // Advance the state machine by the specified time. Returns true if the
    // state machine will continue to animate after this advance.
    bool advance(float seconds);

    // Returns true when the StateMachineInstance has more data to process.
    bool needsAdvance() const;

    // Returns a pointer to the instance's stateMachine
    const StateMachine* stateMachine() const { return m_Machine; }

    size_t inputCount() const override { return m_InputInstances.size(); }
    SMIInput* input(size_t index) const override;
    SMIBool* getBool(const std::string& name) const override;
    SMINumber* getNumber(const std::string& name) const override;
    SMITrigger* getTrigger(const std::string& name) const override;

    size_t currentAnimationCount() const;
    const LinearAnimationInstance* currentAnimationByIndex(size_t index) const;

    // The number of state changes that occurred across all layers on the
    // previous advance.
    size_t stateChangedCount() const;

    // Returns the state name for states that changed in layers on the
    // previously called advance. If the index of out of range, it returns
    // the empty string.
    const LayerState* stateChangedByIndex(size_t index) const;

    bool advanceAndApply(float secs) override;
    std::string name() const override;
    void pointerMove(Vec2D position) override;
    void pointerDown(Vec2D position) override;
    void pointerUp(Vec2D position) override;

    float durationSeconds() const override { return -1; }
    Loop loop() const override { return Loop::oneShot; }
    bool isTranslucent() const override { return true; }

    /// Allow anything referencing a concrete StateMachineInstace access to
    /// the backing artboard (explicitly not allowed on Scenes).
    Artboard* artboard() { return m_ArtboardInstance; }
};
} // namespace rive
#endif
