#ifndef _RIVE_STATE_MACHINE_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INSTANCE_HPP_

#include <string>
#include <stddef.h>
#include <vector>
#include <unordered_map>
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_instance.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/core/field_types/core_callback_type.hpp"
#include "rive/hit_result.hpp"
#include "rive/listener_type.hpp"
#include "rive/nested_animation.hpp"
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
class HitComponent;
class HitShape;
class ListenerGroup;
class NestedArtboard;
class NestedEventListener;
class NestedEventNotifier;
class Event;
class KeyedProperty;
class EventReport;
class DataBind;
class BindableProperty;

#ifdef WITH_RIVE_TOOLS
class StateMachineInstance;
typedef void (*InputChanged)(StateMachineInstance*, uint64_t);
#endif

class StateMachineInstance : public Scene, public NestedEventNotifier, public NestedEventListener
{
    friend class SMIInput;
    friend class KeyedProperty;
    friend class HitComponent;
    friend class StateMachineLayerInstance;

private:
    /// Provide a hitListener if you want to process a down or an up for the pointer position
    /// too.
    HitResult updateListeners(Vec2D position, ListenerType hitListener);

    template <typename SMType, typename InstType>
    InstType* getNamedInput(const std::string& name) const;
    void notifyEventListeners(const std::vector<EventReport>& events, NestedArtboard* source);
    void sortHitComponents();
    double randomValue();
    StateTransition* findRandomTransition(StateInstance* stateFromInstance, bool ignoreTriggers);
    StateTransition* findAllowedTransition(StateInstance* stateFromInstance, bool ignoreTriggers);
    DataContext* m_DataContext;

public:
    StateMachineInstance(const StateMachine* machine, ArtboardInstance* instance);
    StateMachineInstance(StateMachineInstance const&) = delete;
    ~StateMachineInstance() override;

    void markNeedsAdvance();
    // Advance the state machine by the specified time. Returns true if the
    // state machine will continue to animate after this advance.
    bool advance(float seconds);

    // Returns true when the StateMachineInstance has more data to process.
    bool needsAdvance() const;

    // Returns a pointer to the instance's stateMachine
    const StateMachine* stateMachine() const { return m_machine; }

    size_t inputCount() const override { return m_inputInstances.size(); }
    SMIInput* input(size_t index) const override;
    SMIBool* getBool(const std::string& name) const override;
    SMINumber* getNumber(const std::string& name) const override;
    SMITrigger* getTrigger(const std::string& name) const override;
    void dataContextFromInstance(ViewModelInstance* viewModelInstance) override;
    void dataContext(DataContext* dataContext);

    size_t currentAnimationCount() const;
    const LinearAnimationInstance* currentAnimationByIndex(size_t index) const;

    // The number of state changes that occurred across all layers on the
    // previous advance.
    std::size_t stateChangedCount() const;

    // Returns the state name for states that changed in layers on the
    // previously called advance. If the index of out of range, it returns
    // the empty string.
    const LayerState* stateChangedByIndex(size_t index) const;

    bool advanceAndApply(float secs) override;
    std::string name() const override;
    HitResult pointerMove(Vec2D position) override;
    HitResult pointerDown(Vec2D position) override;
    HitResult pointerUp(Vec2D position) override;
    HitResult pointerExit(Vec2D position) override;
#ifdef WITH_RIVE_TOOLS
    bool hitTest(Vec2D position) const;
#endif

    float durationSeconds() const override { return -1; }
    Loop loop() const override { return Loop::oneShot; }
    bool isTranslucent() const override { return true; }

    /// Allow anything referencing a concrete StateMachineInstace access to
    /// the backing artboard (explicitly not allowed on Scenes).
    Artboard* artboard() const { return m_artboardInstance; }

    void setParentStateMachineInstance(StateMachineInstance* instance)
    {
        m_parentStateMachineInstance = instance;
    }
    StateMachineInstance* parentStateMachineInstance() { return m_parentStateMachineInstance; }

    void setParentNestedArtboard(NestedArtboard* artboard) { m_parentNestedArtboard = artboard; }
    NestedArtboard* parentNestedArtboard() { return m_parentNestedArtboard; }
    void notify(const std::vector<EventReport>& events, NestedArtboard* context) override;

    /// Tracks an event that reported, will be cleared at the end of the next advance.
    void reportEvent(Event* event, float secondsDelay = 0.0f) override;

    /// Gets the number of events that reported since the last advance.
    std::size_t reportedEventCount() const;

    /// Gets a reported event at an index < reportedEventCount().
    const EventReport reportedEventAt(std::size_t index) const;
    bool playsAudio() override { return true; }
    BindableProperty* bindablePropertyInstance(BindableProperty* bindableProperty) const;
    DataBind* bindableDataBind(BindableProperty* bindableProperty);
#ifdef TESTING
    size_t hitComponentsCount() { return m_hitComponents.size(); };
    HitComponent* hitComponent(size_t index)
    {
        if (index < m_hitComponents.size())
        {
            return m_hitComponents[index].get();
        }
        return nullptr;
    }
    const LayerState* layerState(size_t index);
#endif
    void updateDataBinds();

private:
    std::vector<EventReport> m_reportedEvents;
    const StateMachine* m_machine;
    bool m_needsAdvance = false;
    std::vector<SMIInput*> m_inputInstances; // we own each pointer
    size_t m_layerCount;
    StateMachineLayerInstance* m_layers;
    std::vector<std::unique_ptr<HitComponent>> m_hitComponents;
    std::vector<std::unique_ptr<ListenerGroup>> m_listenerGroups;
    StateMachineInstance* m_parentStateMachineInstance = nullptr;
    NestedArtboard* m_parentNestedArtboard = nullptr;
    std::vector<DataBind*> m_dataBinds;
    std::unordered_map<BindableProperty*, BindableProperty*> m_bindablePropertyInstances;
    std::unordered_map<BindableProperty*, DataBind*> m_bindableDataBinds;

#ifdef WITH_RIVE_TOOLS
public:
    void onInputChanged(InputChanged callback) { m_inputChangedCallback = callback; }
    InputChanged m_inputChangedCallback = nullptr;
#endif
};

class HitComponent
{
public:
    Component* component() const { return m_component; }
    HitComponent(Component* component, StateMachineInstance* stateMachineInstance) :
        m_component(component), m_stateMachineInstance(stateMachineInstance)
    {}
    virtual ~HitComponent() {}
    virtual HitResult processEvent(Vec2D position, ListenerType hitType, bool canHit) = 0;
    virtual void prepareEvent(Vec2D position, ListenerType hitType) = 0;
#ifdef WITH_RIVE_TOOLS
    virtual bool hitTest(Vec2D position) const = 0;
#endif
#ifdef TESTING
    int earlyOutCount = 0;
#endif

protected:
    Component* m_component;
    StateMachineInstance* m_stateMachineInstance;
};

} // namespace rive
#endif
