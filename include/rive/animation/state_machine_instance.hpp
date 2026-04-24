#ifndef _RIVE_STATE_MACHINE_INSTANCE_HPP_
#define _RIVE_STATE_MACHINE_INSTANCE_HPP_

#include <string>
#include <stddef.h>
#include <vector>
#include <unordered_map>
#include "rive/animation/keyboard_listener_group.hpp"
#include "rive/animation/semantic_listener_group.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_instance.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/core/field_types/core_callback_type.hpp"
#include "rive/hit_result.hpp"
#include "rive/listener_type.hpp"
#include "rive/nested_animation.hpp"
#include "rive/scene.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include "rive/input/focusable.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/semantic/semantic_manager.hpp"

namespace rive
{
class FocusData;
class FocusListenerGroup;
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
class HitDrawable;
class ListenerViewModel;
class ScriptedListenerAction;
typedef void (*DataBindChanged)();

#ifdef WITH_RIVE_TOOLS
class StateMachineInstance;
typedef void (*InputChanged)(StateMachineInstance*, uint64_t);
#endif

class StateMachineInstance : public Scene,
                             public NestedEventNotifier,
                             public NestedEventListener,
                             public DataBindContainer
{
    friend class SMIInput;
    friend class KeyedProperty;
    friend class HitComponent;
    friend class StateMachineLayerInstance;

private:
    /// Provide a hitListener if you want to process a down or an up for the
    /// pointer position too.
    HitResult updateListeners(Vec2D position,
                              ListenerType hitListener,
                              int pointerId = 0,
                              float timeStamp = 0);

    template <typename SMType, typename InstType>
    InstType* getNamedInput(const std::string& name) const;
    void notifyEventListeners(const std::vector<EventReport>& events,
                              NestedArtboard* source);
    void sortHitComponents();
    double randomValue();
    StateTransition* findRandomTransition(
        StateInstance* stateFromInstance,
        StateMachineLayerInstance* layerInstance);
    StateTransition* findAllowedTransition(
        StateInstance* stateFromInstance,
        StateMachineLayerInstance* layerInstance);

    rcp<DataContext> m_DataContext = nullptr;
    void addToHitLookup(Component* target,
                        bool isLayoutComponent,
                        std::unordered_map<Component*, HitDrawable*>& hitLookup,
                        ListenerGroup* listenerGroup,
                        bool isOpaque);

public:
    StateMachineInstance(const StateMachine* machine,
                         ArtboardInstance* instance);
    StateMachineInstance(StateMachineInstance const&) = delete;
    ~StateMachineInstance() override;

    void markNeedsAdvance();
    // Advance the state machine by the specified time. Returns true if the
    // state machine will continue to animate after this advance.
    bool advance(float seconds, bool newFrame);

    bool advance(float seconds) { return advance(seconds, true); }

    // Returns true when the StateMachineInstance has more data to process.
    bool needsAdvance() const;

    void resetState();

    // Returns a pointer to the instance's stateMachine
    const StateMachine* stateMachine() const { return m_machine; }

    size_t inputCount() const override { return m_inputInstances.size(); }
    SMIInput* input(size_t index) const override;
    SMIBool* getBool(const std::string& name) const override;
    SMINumber* getNumber(const std::string& name) const override;
    SMITrigger* getTrigger(const std::string& name) const override;
    void bindViewModelInstance(
        rcp<ViewModelInstance> viewModelInstance) override;
    void bindDataContext(rcp<DataContext> dataContext);
    void dataContext(rcp<DataContext> dataContext);
    rcp<DataContext> dataContext() const { return m_DataContext; }
    void rebind() override;

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
    void advancedDataContext();
    void reset();
    std::string name() const override;
    HitResult pointerMove(Vec2D position,
                          float timeStamp = 0,
                          int pointerId = 0) override;
    HitResult pointerDown(Vec2D position, int pointerId = 0) override;
    HitResult pointerUp(Vec2D position, int pointerId = 0) override;
    HitResult pointerExit(Vec2D position, int pointerId = 0) override;
    HitResult dragStart(Vec2D position,
                        float timeStamp = 0,
                        bool disablePointer = true,
                        int pointerId = 0);
    HitResult dragEnd(Vec2D position, float timeStamp = 0, int pointerId = 0);

    bool tryChangeState();
    bool hitTest(Vec2D position) const;

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
    StateMachineInstance* parentStateMachineInstance()
    {
        return m_parentStateMachineInstance;
    }

    void setParentNestedArtboard(NestedArtboard* artboard)
    {
        m_parentNestedArtboard = artboard;
    }
    NestedArtboard* parentNestedArtboard() { return m_parentNestedArtboard; }
    void notify(const std::vector<EventReport>& events,
                NestedArtboard* context) override;
    void notifyListenerViewModels(
        const std::vector<ListenerViewModel*>& events);

    /// Tracks an event that reported, will be cleared at the end of the next
    /// advance.
    void reportEvent(Event* event, float secondsDelay = 0.0f) override;

    void applyEvents();

    void reportListenerViewModel(ListenerViewModel*);

    /// Gets the number of events that reported since the last advance.
    std::size_t reportedEventCount() const;

    /// Gets a reported event at an index < reportedEventCount().
    const EventReport reportedEventAt(std::size_t index) const;
    bool playsAudio() override { return true; }
    BindableProperty* bindablePropertyInstance(
        BindableProperty* bindableProperty) const;
    DataBind* bindableDataBindToSource(
        BindableProperty* bindableProperty) const;
    DataBind* bindableDataBindToTarget(
        BindableProperty* bindableProperty) const;

    /// Find the per-instance BindablePropertyNumber for the given shared
    /// StateTransition and property key (e.g. durationPropertyKey).
    /// Returns nullptr if no binding exists.
    BindablePropertyNumber* findTransitionPropertyInstance(
        const StateTransition* transition,
        uint32_t propertyKey) const;
    bool hasListeners() { return m_hitComponents.size() > 0; }
    void clearDataContext();
    void relinkDataContext() override;
    void rebuildDataBind(DataBind*) override;
    void internalDataContext(rcp<DataContext> dataContext);
    ScriptedObject* scriptedObject(const ScriptedObject*) const;

    /// Queue a focus event for deferred execution during advance().
    void queueFocusEvent(FocusListenerGroup* group, bool isFocus);

    /// Queue a semantic event for deferred execution during advance().
    void queueSemanticEvent(SemanticListenerGroup* group,
                            SemanticActionType actionType);

    /// Fire a semantic action on the SemanticData with the given node ID.
    void fireSemanticAction(uint32_t semanticNodeId,
                            SemanticActionType actionType);

    /// Get the focus manager for this state machine instance.
    /// Returns the external focus manager if set, otherwise the internal one.
    FocusManager* focusManager()
    {
        return m_externalFocusManager ? m_externalFocusManager
                                      : &m_focusManager;
    }

    /// Check if this state machine is using an external focus manager.
    bool hasExternalFocusManager() const
    {
        return m_externalFocusManager != nullptr;
    }

    /// Get the internal focus manager (always owned by this
    /// StateMachineInstance). Useful when you need to operate only on the
    /// internal manager regardless of whether an external one is set.
    FocusManager* internalFocusManager() { return &m_focusManager; }

    /// Set an external focus manager to use instead of the internal one.
    /// This is used when a nested artboard should share focus with its parent.
    /// If the focus tree was already built with a different manager, it will
    /// be rebuilt with the new manager.
    void setExternalFocusManager(FocusManager* manager);

    /// Set focus to a specific FocusData's node.
    void setFocus(FocusData* focusData);

    /// Get the semantic manager for this state machine instance.
    /// Returns the external manager if set, otherwise the internal one.
    /// Returns nullptr if semantics has not been enabled.
    SemanticManager* semanticManager() const
    {
        return m_externalSemanticManager ? m_externalSemanticManager
                                         : m_semanticManager.get();
    }

    /// Enable semantics for this state machine instance. Creates the
    /// internal SemanticManager (if no external one was set) and builds
    /// the semantic tree. This is a no-op if semantics is already enabled.
    void enableSemantics();

    /// Set an external semantic manager to use instead of the internal one.
    /// Used when a nested artboard should share the parent's semantic tree.
    /// Rebuilds the semantic tree with the new manager if already built.
    /// @param parentNode If provided, the rebuilt tree is attached under it;
    /// nullptr attaches at the manager root.
    void setExternalSemanticManager(SemanticManager* manager,
                                    rcp<SemanticNode> parentNode = nullptr);

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
    void enablePointerEvents(int pointerId = 0);
    void disablePointerEvents(int pointerId = 0);
    void dispose();

private:
    std::vector<EventReport> m_reportedEvents;
    std::vector<EventReport> m_reportingEvents;
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
    std::vector<ListenerViewModel*> m_listenerViewModels;
    std::vector<ListenerViewModel*> m_reportedListenerViewModels;
    std::vector<ListenerViewModel*> m_reportingListenerViewModels;
    std::unordered_map<BindableProperty*, BindableProperty*>
        m_bindablePropertyInstances;
    std::unordered_map<const ScriptedObject*, ScriptedObject*>
        m_scriptedObjectsMap;
    std::unordered_map<BindableProperty*, DataBind*>
        m_bindableDataBindsToTarget;
    std::unordered_map<BindableProperty*, DataBind*>
        m_bindableDataBindsToSource;
    /// Map from shared StateTransition* to per-instance BindablePropertyNumber
    /// instances, keyed by original property key. Data binds write to these
    /// instead of the shared StateTransition object.
    std::unordered_map<const Core*,
                       std::unordered_map<uint32_t, BindablePropertyNumber*>>
        m_transitionPropertyInstances;
    uint8_t m_drawOrderChangeCounter = 0;
    void unbind();
    void removeEventListeners();
    void initScriptedObjects();

    // Focus management
    FocusManager m_focusManager;
    FocusManager* m_externalFocusManager = nullptr;
    std::vector<std::unique_ptr<FocusListenerGroup>> m_focusListenerGroups;
    std::vector<std::unique_ptr<KeyboardListenerGroup>>
        m_keyboardListenerGroups;

    // Semantic management
    std::unique_ptr<SemanticManager> m_semanticManager;
    SemanticManager* m_externalSemanticManager = nullptr;

    // Queued focus events for deferred processing
    struct QueuedFocusEvent
    {
        FocusListenerGroup* group;
        bool isFocus;
    };
    std::vector<QueuedFocusEvent> m_queuedFocusEvents;
    void processFocusEvents();

    // Semantic listener groups and queued events
    std::vector<std::unique_ptr<SemanticListenerGroup>>
        m_semanticListenerGroups;
    struct QueuedSemanticEvent
    {
        SemanticListenerGroup* group;
        SemanticActionType actionType;
    };
    std::vector<QueuedSemanticEvent> m_queuedSemanticEvents;
    void processSemanticEvents();

#ifdef WITH_RIVE_TOOLS
public:
    void onInputChanged(InputChanged callback)
    {
        m_inputChangedCallback = callback;
    }
    void onDataBindChanged(DataBindChanged callback);
    InputChanged m_inputChangedCallback = nullptr;
#endif
};

class HitComponent
{
public:
    Component* component() const { return m_component; }
    HitComponent(Component* component,
                 StateMachineInstance* stateMachineInstance) :
        m_component(component), m_stateMachineInstance(stateMachineInstance)
    {}
    virtual ~HitComponent() {}
    virtual HitResult processEvent(Vec2D position,
                                   ListenerType hitType,
                                   bool canHit,
                                   float timeStamp = 0,
                                   int pointerId = 0) = 0;
    virtual void prepareEvent(Vec2D position,
                              ListenerType hitType,
                              int pointerId) = 0;
    virtual bool hitTest(Vec2D position) const = 0;
    virtual void enablePointerEvents(int pointerId = 0) {}
    virtual void disablePointerEvents(int pointerId = 0) {}
#ifdef TESTING
    int earlyOutCount = 0;
#endif

protected:
    Component* m_component;
    StateMachineInstance* m_stateMachineInstance;
};

} // namespace rive
#endif
