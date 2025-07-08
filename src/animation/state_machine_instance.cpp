#include "rive/animation/animation_reset.hpp"
#include "rive/animation/animation_reset_factory.hpp"
#include "rive/animation/animation_state_instance.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/layer_state_flags.hpp"
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_instance.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/animation/state_machine_fire_event.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/event_report.hpp"
#include "rive/gesture_click_phase.hpp"
#include "rive/hit_result.hpp"
#include "rive/process_event_result.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/hit_test.hpp"
#include "rive/nested_animation.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text.hpp"
#include "rive/math/math_types.hpp"
#include "rive/audio_event.hpp"
#include <unordered_map>
#include <chrono>

using namespace rive;
namespace rive
{
class StateMachineLayerInstance
{
public:
    ~StateMachineLayerInstance()
    {
        delete m_anyStateInstance;
        delete m_currentState;
        delete m_stateFrom;
    }

    void init(StateMachineInstance* stateMachineInstance,
              const StateMachineLayer* layer,
              ArtboardInstance* instance)
    {
        m_stateMachineInstance = stateMachineInstance;
        m_artboardInstance = instance;
        assert(m_layer == nullptr);
        m_anyStateInstance =
            layer->anyState()->makeInstance(instance).release();
        m_layer = layer;
        changeState(m_layer->entryState());

#ifdef TESTING
        srand((unsigned int)1);
#else
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch())
                         .count();
        srand((unsigned int)nanos);
#endif
    }

    void resetState()
    {
        if (m_stateFrom != m_anyStateInstance && m_stateFrom != m_currentState)
        {
            delete m_stateFrom;
        }
        m_stateFrom = nullptr;
        if (m_currentState != m_anyStateInstance)
        {
            delete m_currentState;
        }
        m_currentState = nullptr;
        changeState(m_layer->entryState());
    }

    void updateMix(float seconds)
    {
        if (m_transition != nullptr && m_stateFrom != nullptr &&
            m_transition->duration() != 0)
        {
            m_mix = std::min(
                1.0f,
                std::max(0.0f,
                         (m_mix + seconds / m_transition->mixTime(
                                                m_stateFrom->state()))));
            if (m_mix == 1.0f && !m_transitionCompleted)
            {
                m_transitionCompleted = true;
                clearAnimationReset();
                fireEvents(StateMachineFireOccurance::atEnd,
                           m_transition->events());
            }
        }
        else
        {
            m_mix = 1.0f;
        }
    }

    bool advance(float seconds, bool newFrame)
    {
        if (newFrame)
        {
            m_stateMachineChangedOnAdvance = false;
        }
        m_currentState->advance(seconds, m_stateMachineInstance);
        updateMix(seconds);

        if (m_stateFrom != nullptr && m_mix < 1.0f && !m_holdAnimationFrom)
        {
            // This didn't advance during our updateState, but it should now
            // that we realize we need to mix it in.
            m_stateFrom->advance(seconds, m_stateMachineInstance);
        }

        apply();

        bool changedState = false;

        for (int i = 0; updateState(); i++)
        {
            changedState = true;
            apply();

            if (i == maxIterations)
            {
                fprintf(stderr, "StateMachine exceeded max iterations.\n");
                return false;
            }
        }

        m_currentState->clearSpilledTime();

        return changedState || m_mix != 1.0f || m_waitingForExit ||
               (m_currentState != nullptr && m_currentState->keepGoing());
    }

    bool isTransitioning()
    {
        return m_transition != nullptr && m_stateFrom != nullptr &&
               m_transition->duration() != 0 && m_mix < 1.0f;
    }

    bool updateState()
    {
        // Don't allow changing state while a transition is taking place
        // (we're mixing one state onto another) if enableEarlyExit is not true.
        if (isTransitioning() && !m_transition->enableEarlyExit())
        {
            return false;
        }

        m_waitingForExit = false;

        if (tryChangeState(m_anyStateInstance))
        {
            return true;
        }

        return tryChangeState(m_currentState);
    }

    void fireEvents(StateMachineFireOccurance occurs,
                    const std::vector<StateMachineFireEvent*>& fireEvents)
    {
        for (auto event : fireEvents)
        {
            if (event->occurs() == occurs)
            {
                event->perform(m_stateMachineInstance);
            }
        }
    }

    bool canChangeState(const LayerState* stateTo)
    {
        return !(
            (m_currentState == nullptr ? nullptr : m_currentState->state()) ==
            stateTo);
    }

    double randomValue()
    {
#ifdef TESTING
        return 0;
#else
        return ((double)rand() / (RAND_MAX));
#endif
    }

    bool changeState(const LayerState* stateTo)
    {
        if ((m_currentState == nullptr ? nullptr : m_currentState->state()) ==
            stateTo)
        {
            return false;
        }

        // Fire end events for the state we're changing from.
        if (m_currentState != nullptr)
        {
            fireEvents(StateMachineFireOccurance::atEnd,
                       m_currentState->state()->events());
        }

        m_currentState =
            stateTo == nullptr
                ? nullptr
                : stateTo->makeInstance(m_artboardInstance).release();

        // Fire start events for the state we're changing to.
        if (m_currentState != nullptr)
        {
            fireEvents(StateMachineFireOccurance::atStart,
                       m_currentState->state()->events());
        }
        return true;
    }

    StateTransition* findRandomTransition(StateInstance* stateFromInstance)
    {
        uint32_t totalWeight = 0;
        auto stateFrom = stateFromInstance->state();
        for (size_t i = 0, length = stateFrom->transitionCount(); i < length;
             i++)
        {
            auto transition = stateFrom->transition(i);
            if (canChangeState(transition->stateTo()))
            {

                auto allowed = transition->allowed(stateFromInstance,
                                                   m_stateMachineInstance,
                                                   this);
                if (allowed == AllowTransition::yes)
                {
                    transition->evaluatedRandomWeight(
                        transition->randomWeight());
                    totalWeight += transition->randomWeight();
                }
                else
                {
                    transition->evaluatedRandomWeight(0);
                    if (allowed == AllowTransition::waitingForExit)
                    {
                        m_waitingForExit = true;
                    }
                }
            }
            else
            {
                transition->evaluatedRandomWeight(0);
            }
        }
        if (totalWeight > 0)
        {
            double randomWeight = randomValue() * totalWeight * 1.0;
            double currentWeight = 0;
            size_t index = 0;
            StateTransition* transition;
            while (index < stateFrom->transitionCount())
            {
                transition = stateFrom->transition(index);
                double transitionWeight =
                    (double)transition->evaluatedRandomWeight();
                if (currentWeight + transitionWeight > randomWeight)
                {
                    transition->useLayerInConditions(m_stateMachineInstance,
                                                     this);
                    return transition;
                }
                currentWeight += transitionWeight;
                index++;
            }
        }
        return nullptr;
    }

    StateTransition* findAllowedTransition(StateInstance* stateFromInstance)
    {
        auto stateFrom = stateFromInstance->state();
        // If it should randomize
        if ((static_cast<LayerStateFlags>(stateFrom->flags()) &
             LayerStateFlags::Random) == LayerStateFlags::Random)
        {
            return findRandomTransition(stateFromInstance);
        }
        // Else search the first valid transition
        for (size_t i = 0, length = stateFrom->transitionCount(); i < length;
             i++)
        {
            auto transition = stateFrom->transition(i);
            if (canChangeState(transition->stateTo()))
            {

                auto allowed = transition->allowed(stateFromInstance,
                                                   m_stateMachineInstance,
                                                   this);
                if (allowed == AllowTransition::yes)
                {
                    transition->evaluatedRandomWeight(
                        transition->randomWeight());
                    transition->useLayerInConditions(m_stateMachineInstance,
                                                     this);
                    return transition;
                }
                else
                {
                    transition->evaluatedRandomWeight(0);
                    if (allowed == AllowTransition::waitingForExit)
                    {
                        m_waitingForExit = true;
                    }
                }
            }
        }
        return nullptr;
    }

    void buildAnimationResetForTransition()
    {
        m_animationReset =
            AnimationResetFactory::fromStates(m_stateFrom,
                                              m_currentState,
                                              m_artboardInstance);
    }

    void clearAnimationReset()
    {
        if (m_animationReset != nullptr)
        {
            AnimationResetFactory::release(std::move(m_animationReset));
            m_animationReset = nullptr;
        }
    }

    bool tryChangeState(StateInstance* stateFromInstance)
    {
        if (stateFromInstance == nullptr)
        {
            return false;
        }
        auto outState = m_currentState;
        auto transition = findAllowedTransition(stateFromInstance);
        if (transition != nullptr)
        {
            clearAnimationReset();
            changeState(transition->stateTo());
            m_stateMachineChangedOnAdvance = true;
            // state actually has changed
            m_transition = transition;
            fireEvents(StateMachineFireOccurance::atStart,
                       transition->events());
            if (transition->duration() == 0)
            {
                m_transitionCompleted = true;
                fireEvents(StateMachineFireOccurance::atEnd,
                           transition->events());
            }
            else
            {
                m_transitionCompleted = false;
            }

            if (m_stateFrom != m_anyStateInstance)
            {
                // Old state from is done.
                delete m_stateFrom;
            }
            m_stateFrom = outState;

            if (!m_transitionCompleted)
            {
                buildAnimationResetForTransition();
            }

            // If we had an exit time and wanted to pause on exit, make
            // sure to hold the exit time. Delegate this to the
            // transition by telling it that it was completed.
            if (outState != nullptr && transition->applyExitCondition(outState))
            {
                // Make sure we apply this state. This only returns true
                // when it's an animation state instance.
                auto instance =
                    static_cast<AnimationStateInstance*>(m_stateFrom)
                        ->animationInstance();

                m_holdAnimation = instance->animation();
                m_holdTime = instance->time();
            }
            m_mixFrom = m_mix;

            // Keep mixing last animation that was mixed in.
            if (m_mix != 0.0f)
            {
                m_holdAnimationFrom = transition->pauseOnExit();
            }
            if (m_stateFrom != nullptr &&
                m_stateFrom->state()->is<AnimationState>() &&
                m_currentState != nullptr)
            {
                auto instance =
                    static_cast<AnimationStateInstance*>(m_stateFrom)
                        ->animationInstance();

                auto spilledTime = instance->spilledTime();
                m_currentState->advance(spilledTime, m_stateMachineInstance);
            }
            m_mix = 0.0f;
            updateMix(0.0f);
            m_waitingForExit = false;
            return true;
        }
        return false;
    }

    void apply(/*Artboard* artboard*/)
    {
        if (m_animationReset != nullptr)
        {
            m_animationReset->apply(m_artboardInstance);
        }
        if (m_holdAnimation != nullptr)
        {
            m_holdAnimation->apply(m_artboardInstance, m_holdTime, m_mixFrom);
            m_holdAnimation = nullptr;
        }

        KeyFrameInterpolator* interpolator = nullptr;
        if (m_transition != nullptr && m_transition->interpolator() != nullptr)
        {
            interpolator = m_transition->interpolator();
        }

        if (m_stateFrom != nullptr && m_mix < 1.0f)
        {
            auto fromMix = interpolator != nullptr
                               ? interpolator->transform(m_mixFrom)
                               : m_mixFrom;
            m_stateFrom->apply(m_artboardInstance, fromMix);
        }
        if (m_currentState != nullptr)
        {
            auto mix = interpolator != nullptr ? interpolator->transform(m_mix)
                                               : m_mix;
            m_currentState->apply(m_artboardInstance, mix);
        }
    }

    bool stateChangedOnAdvance() const
    {
        return m_stateMachineChangedOnAdvance;
    }

    const LayerState* currentState()
    {
        return m_currentState == nullptr ? nullptr : m_currentState->state();
    }

    const LinearAnimationInstance* currentAnimation() const
    {
        if (m_currentState == nullptr ||
            !m_currentState->state()->is<AnimationState>())
        {
            return nullptr;
        }
        return static_cast<AnimationStateInstance*>(m_currentState)
            ->animationInstance();
    }

private:
    static const int maxIterations = 100;
    StateMachineInstance* m_stateMachineInstance = nullptr;
    const StateMachineLayer* m_layer = nullptr;
    ArtboardInstance* m_artboardInstance = nullptr;

    StateInstance* m_anyStateInstance = nullptr;
    StateInstance* m_currentState = nullptr;
    StateInstance* m_stateFrom = nullptr;

    const StateTransition* m_transition = nullptr;
    std::unique_ptr<AnimationReset> m_animationReset = nullptr;
    bool m_transitionCompleted = false;

    bool m_holdAnimationFrom = false;

    float m_mix = 1.0f;
    float m_mixFrom = 1.0f;
    bool m_stateMachineChangedOnAdvance = false;

    bool m_waitingForExit = false;
    /// Used to ensure a specific animation is applied on the next apply.
    const LinearAnimation* m_holdAnimation = nullptr;
    float m_holdTime = 0.0f;
};

class ListenerGroup
{
public:
    ListenerGroup(const StateMachineListener* listener) : m_listener(listener)
    {}
    virtual ~ListenerGroup() {}
    void consume() { m_isConsumed = true; }
    //
    void hover() { m_isHovered = true; }
    void unhover() { m_isHovered = false; }
    void reset()
    {
        m_isConsumed = false;
        m_prevIsHovered = m_isHovered;
        m_isHovered = false;
        if (m_clickPhase == GestureClickPhase::clicked)
        {
            m_clickPhase = GestureClickPhase::out;
        }
    }
    bool isConsumed() { return m_isConsumed; }
    bool isHovered() { return m_isHovered; }
    bool prevHovered() { return m_prevIsHovered; }

    virtual bool canEarlyOut(Component* drawable)
    {
        auto listenerType = m_listener->listenerType();
        return !(listenerType == ListenerType::enter ||
                 listenerType == ListenerType::exit ||
                 listenerType == ListenerType::move);
    }

    virtual bool needsDownListener(Component* drawable)
    {
        auto listenerType = m_listener->listenerType();
        return listenerType == ListenerType::down ||
               listenerType == ListenerType::click;
    }

    virtual bool needsUpListener(Component* drawable)
    {
        auto listenerType = m_listener->listenerType();
        return listenerType == ListenerType::up ||
               listenerType == ListenerType::click;
    }
    // Vec2D position, ListenerType hitType, bool canHit
    virtual ProcessEventResult processEvent(
        Component* component,
        Vec2D position,
        ListenerType hitEvent,
        bool canHit,
        float timeStamp,
        StateMachineInstance* stateMachineInstance)
    {
        // Because each group is tested individually for its hover state, a
        // group could be marked "incorrectly" as hovered at this point. But
        // once we iterate each element in the drawing order, that group can be
        // occluded by an opaque target on top  of it. So although it is hovered
        // in isolation, it shouldn't be considered as hovered in the full
        // context. In this case, we unhover the group so it is not marked as
        // previously hovered.
        if (!canHit && isHovered())
        {
            unhover();
        }

        bool isGroupHovered = canHit ? isHovered() : false;
        bool hoverChange = prevHovered() != isGroupHovered;
        // If hover has changes, it means that the element is hovered for the
        // first time. Previous positions need to be reset to avoid jumps.
        if (hoverChange && isGroupHovered)
        {
            previousPosition.x = position.x;
            previousPosition.y = position.y;
        }

        // Handle click gesture phases. A click gesture has two phases.
        // First one attached to a pointer down actions, second one attached to
        // a pointer up action. Both need to act on a shape of the listener
        // group.
        if (isGroupHovered)
        {
            if (hitEvent == ListenerType::down)
            {
                clickPhase(GestureClickPhase::down);
            }
            else if (hitEvent == ListenerType::up &&
                     clickPhase() == GestureClickPhase::down)
            {
                clickPhase(GestureClickPhase::clicked);
            }
        }
        else
        {
            if (hitEvent == ListenerType::down || hitEvent == ListenerType::up)
            {
                clickPhase(GestureClickPhase::out);
            }
        }
        auto _listener = listener();
        // Always update hover states regardless of which specific listener type
        // we're trying to trigger.
        // If hover has changed and:
        // - it's hovering and the listener is of type enter
        // - it's not hovering and the listener is of type exit
        if (hoverChange && ((isGroupHovered && _listener->listenerType() ==
                                                   ListenerType::enter) ||
                            (!isGroupHovered &&
                             _listener->listenerType() == ListenerType::exit)))
        {
            _listener->performChanges(stateMachineInstance,
                                      position,
                                      previousPosition);
            stateMachineInstance->markNeedsAdvance();
            consume();
        }
        // Perform changes if:
        // - the click gesture is complete and the listener is of type click
        // - the event type matches the listener type and it is hovering the
        // group
        if ((clickPhase() == GestureClickPhase::clicked &&
             _listener->listenerType() == ListenerType::click) ||
            (isGroupHovered && hitEvent == _listener->listenerType()))
        {
            _listener->performChanges(stateMachineInstance,
                                      position,
                                      previousPosition);
            stateMachineInstance->markNeedsAdvance();
            consume();
        }
        previousPosition.x = position.x;
        previousPosition.y = position.y;
        return ProcessEventResult::pointer;
    }
    void clickPhase(GestureClickPhase value) { m_clickPhase = value; }
    GestureClickPhase clickPhase() { return m_clickPhase; }
    const StateMachineListener* listener() const { return m_listener; };
    // A vector storing the previous position for this specific listener gorup
    Vec2D previousPosition;

private:
    // Consumed listeners aren't processed again in the current frame
    bool m_isConsumed = false;
    // This variable holds the hover status of the the listener itself so it can
    // be shared between all shapes that target it
    bool m_isHovered = false;
    // Variable storing the previous hovered state to check for hover changes
    bool m_prevIsHovered = false;
    // A click gesture is composed of three phases and is shared between all
    // shapes
    GestureClickPhase m_clickPhase = GestureClickPhase::out;
    const StateMachineListener* m_listener;
};

class DraggableConstraintListenerGroup : public ListenerGroup
{
public:
    DraggableConstraintListenerGroup(const StateMachineListener* listener,
                                     DraggableConstraint* constraint,
                                     DraggableProxy* draggable) :
        ListenerGroup(listener),
        m_constraint(constraint),
        m_draggable(draggable)
    {}
    ~DraggableConstraintListenerGroup()
    {
        delete listener();
        delete m_draggable;
    }

    DraggableConstraint* constraint() { return m_constraint; }

    bool canEarlyOut(Component* drawable) override { return false; }

    bool needsDownListener(Component* drawable) override { return true; }

    bool needsUpListener(Component* drawable) override { return true; }
    // Vec2D position, ListenerType hitType, bool canHit
    ProcessEventResult processEvent(
        Component* component,
        Vec2D position,
        ListenerType hitEvent,
        bool canHit,
        float timeStamp,
        StateMachineInstance* stateMachineInstance) override
    {
        auto prevPhase = clickPhase();
        ListenerGroup::processEvent(component,
                                    position,
                                    hitEvent,
                                    canHit,
                                    timeStamp,
                                    stateMachineInstance);
        if (prevPhase == GestureClickPhase::down &&
            (clickPhase() == GestureClickPhase::clicked ||
             clickPhase() == GestureClickPhase::out))
        {
            m_draggable->endDrag(position, timeStamp);
            if (hasScrolled)
            {
                return ProcessEventResult::scroll;
                hasScrolled = false;
            }
        }
        else if (prevPhase != GestureClickPhase::down &&
                 clickPhase() == GestureClickPhase::down)
        {
            m_draggable->startDrag(position, timeStamp);
            hasScrolled = false;
        }
        else if (hitEvent == ListenerType::move &&
                 clickPhase() == GestureClickPhase::down)
        {
            m_draggable->drag(position, timeStamp);
            hasScrolled = true;
            return ProcessEventResult::scroll;
        }
        return ProcessEventResult::none;
    }

private:
    DraggableConstraint* m_constraint;
    DraggableProxy* m_draggable;
    bool hasScrolled = false;
};

/// Representation of a Component from the Artboard Instance and all the
/// listeners it triggers. Allows tracking hover and performing hit detection
/// only once on components that trigger multiple listeners.
class HitDrawable : public HitComponent
{
public:
    HitDrawable(Drawable* drawable,
                Component* component,
                StateMachineInstance* stateMachineInstance,
                bool isOpaque) :
        HitComponent(component, stateMachineInstance)
    {
        this->m_drawable = drawable;
        this->isOpaque = isOpaque;
        if (drawable->isTargetOpaque())
        {
            canEarlyOut = false;
        }
    }
    float hitRadius = 2;
    bool isHovered = false;
    bool canEarlyOut = true;
    bool hasDownListener = false;
    bool hasUpListener = false;
    bool isOpaque = false;
    Drawable* m_drawable;
    std::vector<ListenerGroup*> listeners;

    virtual bool hitTestHelper(Vec2D position) const { return false; }

    bool hitTest(Vec2D position) const override
    {
        return hitTestHelper(position);
    }

    virtual bool testBounds(Component* component,
                            Vec2D position,
                            bool skipOnUnclipped) const
    {
        if (component == nullptr)
        {
            return true;
        }

        if (component->is<Drawable>())
        {
            component = component->as<Drawable>()->hittableComponent();
            if (component == nullptr)
            {
                return true;
            }
        }

        if (component->is<LayoutComponent>())
        {
            Mat2D inverseWorld;
            auto layout = component->as<LayoutComponent>();

            if (layout->worldTransform().invert(&inverseWorld))
            {
                // If the layout is not clipped and skipOnUnclipped is true, we
                // don't care about whether it contains the position
                auto canSkip = skipOnUnclipped && !layout->clip();
                if (!canSkip)
                {
                    auto localWorld = inverseWorld * position;
                    if (layout->is<Artboard>())
                    {
                        auto artboard = layout->as<Artboard>();
                        if (artboard->originX() != 0 ||
                            artboard->originY() != 0)
                        {
                            localWorld += Vec2D(
                                artboard->originX() * artboard->layoutWidth(),
                                artboard->originY() * artboard->layoutHeight());
                        }
                    }
                    if (!layout->localBounds().contains(localWorld))
                    {
                        return false;
                    }
                }
                return testBounds(layout->parent(), position, true);
            }
            return false;
        }

        // Keep going
        return testBounds(component->parent(), position, skipOnUnclipped);
    }

    void prepareEvent(Vec2D position, ListenerType hitType) override
    {
        if (canEarlyOut &&
            (hitType != ListenerType::down || !hasDownListener) &&
            (hitType != ListenerType::up || !hasUpListener))
        {
#ifdef TESTING
            earlyOutCount++;
#endif
            return;
        }
        isHovered = hitTest(position);

        // // iterate all listeners associated with this hit shape
        if (isHovered)
        {
            for (auto listenerGroup : listeners)
            {

                listenerGroup->hover();
            }
        }
    }

    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp) override
    {
        // If the shape doesn't have any ListenerType::move / enter / exit and
        // the event being processed is not of the type it needs to handle.
        // There is no need to perform a hitTest (which is relatively expensive
        // and would be happening on every pointer move) so we early out.
        if (canEarlyOut &&
            (hitType != ListenerType::down || !hasDownListener) &&
            (hitType != ListenerType::up || !hasUpListener))
        {
            return HitResult::none;
        }
        bool isBlockingEvent = false;
        // // iterate all listeners associated with this hit shape
        for (auto listenerGroup : listeners)
        {
            if (listenerGroup->isConsumed())
            {
                continue;
            }
            if (listenerGroup->processEvent(m_component,
                                            position,
                                            hitType,
                                            canHit,
                                            timeStamp,
                                            m_stateMachineInstance) ==
                ProcessEventResult::scroll)
            {
                isBlockingEvent = true;
            }
        }
        return (isHovered && canHit)
                   ? (isOpaque || m_drawable->isTargetOpaque() ||
                      isBlockingEvent)
                         ? HitResult::hitOpaque
                         : HitResult::hit
                   : HitResult::none;
    }

    void addListener(ListenerGroup* listenerGroup)
    {
        if (!listenerGroup->canEarlyOut(m_component))
        {
            canEarlyOut = false;
        }
        else
        {
            if (listenerGroup->needsDownListener(m_component))
            {
                hasDownListener = true;
            }
            if (listenerGroup->needsUpListener(m_component))
            {
                hasUpListener = true;
            }
        }
        listeners.push_back(listenerGroup);
    }
};

/// Representation of a HitDrawable with a Hittable component
class HitExpandable : public HitDrawable
{
public:
    bool testBounds(Component* component,
                    Vec2D position,
                    bool skipOnUnclipped) const override
    {
        Hittable* hittable = component ? Hittable::from(component) : nullptr;
        if (hittable != nullptr)
        {
            if (hittable->hitTestAABB(position) &&
                HitDrawable::testBounds(component->parent(), position, true))
            {
                return hittable->hitTestHiFi(position, hitRadius);
            }
            return false;
        }
        return HitDrawable::testBounds(component, position, true);
    }

    HitExpandable(Drawable* drawable,
                  Component* component,
                  StateMachineInstance* stateMachineInstance,
                  bool isOpaque = false) :
        HitDrawable(drawable, component, stateMachineInstance, isOpaque)
    {}

    bool hitTestHelper(Vec2D position) const override
    {
        return testBounds(m_component, position, true);
    }
};

class HitTextRun : public HitExpandable
{
public:
    HitTextRun(Drawable* drawable,
               TextValueRun* component,
               StateMachineInstance* stateMachineInstance,
               bool isOpaque = false) :
        HitExpandable(drawable, component, stateMachineInstance, isOpaque)
    {
        if (component)
        {
            component->isHitTarget(true);
        }
    }
};

class HitLayout : public HitDrawable
{
public:
    HitLayout(Drawable* layout,
              StateMachineInstance* stateMachineInstance,
              bool isOpaque = false) :
        HitDrawable(layout, layout, stateMachineInstance, isOpaque)
    {}

    bool hitTestHelper(Vec2D position) const override
    {
        return testBounds(m_component, position, false);
    }
};

class HitNestedArtboard : public HitComponent
{
public:
    HitNestedArtboard(Component* nestedArtboard,
                      StateMachineInstance* stateMachineInstance) :
        HitComponent(nestedArtboard, stateMachineInstance)
    {}
    ~HitNestedArtboard() override {}

    bool hitTest(Vec2D position) const override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        if (nestedArtboard->isCollapsed())
        {
            return false;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            // Mounted artboard isn't ready or has a 0 scale transform.
            return false;
        }

        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>())
            {
                auto nestedStateMachine =
                    nestedAnimation->as<NestedStateMachine>();
                if (nestedStateMachine->hitTest(nestedPosition))
                {
                    return true;
                }
            }
        }
        return false;
    }
    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp) override
    {
        auto nestedArtboard = m_component->as<NestedArtboard>();
        HitResult hitResult = HitResult::none;
        if (nestedArtboard->isCollapsed())
        {
            return hitResult;
        }
        Vec2D nestedPosition;
        if (!nestedArtboard->worldToLocal(position, &nestedPosition))
        {
            // Mounted artboard isn't ready or has a 0 scale transform.
            return hitResult;
        }

        for (auto nestedAnimation : nestedArtboard->nestedAnimations())
        {
            if (nestedAnimation->is<NestedStateMachine>())
            {
                auto nestedStateMachine =
                    nestedAnimation->as<NestedStateMachine>();
                if (canHit)
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                            hitResult =
                                nestedStateMachine->pointerDown(nestedPosition);
                            break;
                        case ListenerType::up:
                            hitResult =
                                nestedStateMachine->pointerUp(nestedPosition);
                            break;
                        case ListenerType::move:
                            hitResult =
                                nestedStateMachine->pointerMove(nestedPosition,
                                                                timeStamp);
                            break;
                        case ListenerType::enter:
                        case ListenerType::exit:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::draggableConstraint:
                            break;
                    }
                }
                else
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                        case ListenerType::up:
                        case ListenerType::move:
                            nestedStateMachine->pointerExit(nestedPosition);
                            break;
                        case ListenerType::enter:
                        case ListenerType::exit:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::draggableConstraint:
                            break;
                    }
                }
            }
        }
        return hitResult;
    }
    void prepareEvent(Vec2D position, ListenerType hitType) override {}
};

class HitComponentList : public HitComponent
{
public:
    HitComponentList(Component* componentList,
                     StateMachineInstance* stateMachineInstance) :
        HitComponent(componentList, stateMachineInstance)
    {}
    ~HitComponentList() override {}

    bool hitTest(Vec2D position) const override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        if (componentList->isCollapsed())
        {
            return false;
        }
        for (int i = 0; i < componentList->artboardCount(); i++)
        {
            Vec2D listPosition;
            if (!componentList->worldToLocal(position, &listPosition, i))
            {
                // Mounted artboard isn't ready or has a 0 scale transform.
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr && stateMachine->hitTest(listPosition))
            {
                return true;
            }
        }
        return false;
    }
    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp) override
    {
        auto componentList = m_component->as<ArtboardComponentList>();
        HitResult hitResult = HitResult::none;
        bool runningCanHit = canHit;
        if (componentList->isCollapsed())
        {
            return hitResult;
        }
        for (int i = 0; i < componentList->artboardCount(); i++)
        {
            Vec2D listPosition;
            bool hit = componentList->worldToLocal(position, &listPosition, i);
            if (!hit)
            {
                continue;
            }
            auto stateMachine = componentList->stateMachineInstance(i);
            if (stateMachine != nullptr)
            {
                HitResult itemHitResult = HitResult::none;
                if (runningCanHit)
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                            itemHitResult =
                                stateMachine->pointerDown(listPosition);
                            break;
                        case ListenerType::up:
                            itemHitResult =
                                stateMachine->pointerUp(listPosition);
                            break;
                        case ListenerType::move:
                            itemHitResult =
                                stateMachine->pointerMove(listPosition);
                            break;
                        case ListenerType::enter:
                        case ListenerType::exit:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::draggableConstraint:
                            break;
                    }
                }
                else
                {
                    switch (hitType)
                    {
                        case ListenerType::down:
                        case ListenerType::up:
                        case ListenerType::move:
                            stateMachine->pointerExit(listPosition);
                            break;
                        case ListenerType::enter:
                        case ListenerType::exit:
                        case ListenerType::event:
                        case ListenerType::click:
                        case ListenerType::draggableConstraint:
                            break;
                    }
                }
                if ((hitResult == HitResult::none &&
                     (itemHitResult == HitResult::hit ||
                      itemHitResult == HitResult::hitOpaque)) ||
                    (hitResult == HitResult::hit &&
                     itemHitResult == HitResult::hitOpaque))
                {
                    hitResult = itemHitResult;
                }
                if (hitResult == HitResult::hitOpaque)
                {
                    runningCanHit = false;
                }
            }
        }
        return hitResult;
    }
    void prepareEvent(Vec2D position, ListenerType hitType) override {}
};

} // namespace rive

HitResult StateMachineInstance::updateListeners(Vec2D position,
                                                ListenerType hitType,
                                                float timeStamp)
{
    if (m_artboardInstance->frameOrigin())
    {
        position -= Vec2D(
            m_artboardInstance->originX() * m_artboardInstance->layoutWidth(),
            m_artboardInstance->originY() * m_artboardInstance->layoutHeight());
    }
    // First reset all listener groups before processing the events
    for (const auto& listenerGroup : m_listenerGroups)
    {
        listenerGroup.get()->reset();
    }
    // Next prepare the event to set the common hover status for each group
    for (const auto& hitShape : m_hitComponents)
    {
        hitShape->prepareEvent(position, hitType);
    }
    bool hitSomething = false;
    bool hitOpaque = false;
    // Finally process the events
    for (const auto& hitShape : m_hitComponents)
    {
        // TODO: quick reject.

        HitResult hitResult =
            hitShape->processEvent(position, hitType, !hitOpaque, timeStamp);
        if (hitResult != HitResult::none)
        {
            hitSomething = true;
            if (hitResult == HitResult::hitOpaque)
            {
                hitOpaque = true;
            }
        }
    }
    return hitSomething ? hitOpaque ? HitResult::hitOpaque : HitResult::hit
                        : HitResult::none;
}

bool StateMachineInstance::hitTest(Vec2D position) const
{
    if (m_artboardInstance->frameOrigin())
    {
        position -= Vec2D(
            m_artboardInstance->originX() * m_artboardInstance->layoutWidth(),
            m_artboardInstance->originY() * m_artboardInstance->layoutHeight());
    }

    for (const auto& hitShape : m_hitComponents)
    {
        // TODO: quick reject.

        if (hitShape->hitTest(position))
        {
            return true;
        }
    }
    return false;
}

HitResult StateMachineInstance::pointerMove(Vec2D position, float timeStamp)
{
    return updateListeners(position, ListenerType::move, timeStamp);
}
HitResult StateMachineInstance::pointerDown(Vec2D position)
{
    return updateListeners(position, ListenerType::down);
}
HitResult StateMachineInstance::pointerUp(Vec2D position)
{
    return updateListeners(position, ListenerType::up);
}
HitResult StateMachineInstance::pointerExit(Vec2D position)
{
    return updateListeners(position, ListenerType::exit);
}

#ifdef TESTING
const LayerState* StateMachineInstance::layerState(size_t index)
{
    if (index < m_machine->layerCount())
    {
        return m_layers[index].currentState();
    }
    return nullptr;
}
#endif

void StateMachineInstance::addToHitLookup(
    Component* target,
    bool isLayoutComponent,
    std::unordered_map<Component*, HitDrawable*>& hitLookup,
    ListenerGroup* listenerGroup,
    bool isOpaque)
{
    // target could either be a LayoutComponent or a DrawableProxy
    if (isLayoutComponent)
    {
        HitLayout* hitLayout;
        auto itr = hitLookup.find(target);
        if (itr == hitLookup.end())
        {
            auto hs = rivestd::make_unique<HitLayout>(target->as<Drawable>(),
                                                      this,
                                                      isOpaque);
            hitLookup[target] = hitLayout = hs.get();
            m_hitComponents.push_back(std::move(hs));
        }
        else
        {
            hitLayout = static_cast<HitLayout*>(itr->second);
        }
        hitLayout->addListener(listenerGroup);
        if (isOpaque)
        {
            hitLayout->isOpaque = true;
        }
        return;
    }

    if (target->is<Shape>())
    {
        HitExpandable* hitShape;
        auto itr = hitLookup.find(target);
        if (itr == hitLookup.end())
        {
            Shape* shape = target->as<Shape>();
            shape->addFlags(PathFlags::neverDeferUpdate);
            shape->addDirt(ComponentDirt::Path, true);
            auto hs = rivestd::make_unique<HitExpandable>(shape, shape, this);
            hitLookup[target] = hitShape = hs.get();
            m_hitComponents.push_back(std::move(hs));
        }
        else
        {
            hitShape = static_cast<HitExpandable*>(itr->second);
        }
        hitShape->addListener(listenerGroup);
        return;
    }

    if (target->is<TextValueRun>())
    {
        HitTextRun* hitTextRun;
        auto itr = hitLookup.find(target);
        if (itr == hitLookup.end())
        {
            TextValueRun* run = target->as<TextValueRun>();
            run->textComponent()->addDirt(ComponentDirt::Path, true);
            auto hs = rivestd::make_unique<HitTextRun>(run->textComponent(),
                                                       run,
                                                       this);
            hitLookup[target] = hitTextRun = hs.get();
            m_hitComponents.push_back(std::move(hs));
        }
        else
        {
            hitTextRun = static_cast<HitTextRun*>(itr->second);
        }
        hitTextRun->addListener(listenerGroup);
        return;
    }

    if (target->is<ContainerComponent>())
    {
        target->as<ContainerComponent>()->forEachChild([&](Component* child) {
            addToHitLookup(child,
                           child->is<LayoutComponent>(),
                           hitLookup,
                           listenerGroup,
                           isOpaque);
            return false;
        });
        return;
    }
}

StateMachineInstance::StateMachineInstance(const StateMachine* machine,
                                           ArtboardInstance* instance) :
    Scene(instance), m_machine(machine)
{
    const auto count = machine->inputCount();
    m_inputInstances.resize(count);
    for (size_t i = 0; i < count; i++)
    {
        auto input = machine->input(i);
        if (input == nullptr)
        {
            continue;
        }
        switch (input->coreType())
        {
            case StateMachineBool::typeKey:
                m_inputInstances[i] =
                    new SMIBool(input->as<StateMachineBool>(), this);
                break;
            case StateMachineNumber::typeKey:
                m_inputInstances[i] =
                    new SMINumber(input->as<StateMachineNumber>(), this);
                break;
            case StateMachineTrigger::typeKey:
                m_inputInstances[i] =
                    new SMITrigger(input->as<StateMachineTrigger>(), this);
                break;
            default:
                // Sanity check.
                break;
        }
#ifdef WITH_RIVE_TOOLS
        auto instance = m_inputInstances[i];
        if (instance != nullptr)
        {
            instance->m_index = i;
        }
#endif
    }

    m_layerCount = machine->layerCount();
    m_layers = new StateMachineLayerInstance[m_layerCount];
    for (size_t i = 0; i < m_layerCount; i++)
    {
        m_layers[i].init(this, machine->layer(i), m_artboardInstance);
    }

    // Initialize dataBinds. All databinds are cloned for the state machine
    // instance. That enables binding each instance to its own context without
    // polluting the rest.
    auto dataBindCount = machine->dataBindCount();
    for (size_t i = 0; i < dataBindCount; i++)
    {
        auto dataBind = machine->dataBind(i);
        auto dataBindClone = static_cast<DataBind*>(dataBind->clone());
        dataBindClone->file(dataBind->file());
        if (dataBind->converter() != nullptr)
        {
            dataBindClone->converter(
                dataBind->converter()->clone()->as<DataConverter>());
        }
        m_dataBinds.push_back(dataBindClone);
        if (dataBind->target()->is<BindableProperty>())
        {
            auto bindableProperty = dataBind->target()->as<BindableProperty>();
            auto bindablePropertyInstance =
                m_bindablePropertyInstances.find(bindableProperty);
            BindableProperty* bindablePropertyClone;
            if (bindablePropertyInstance == m_bindablePropertyInstances.end())
            {
                bindablePropertyClone =
                    bindableProperty->clone()->as<BindableProperty>();
                m_bindablePropertyInstances[bindableProperty] =
                    bindablePropertyClone;
            }
            else
            {
                bindablePropertyClone = bindablePropertyInstance->second;
            }
            dataBindClone->target(bindablePropertyClone);
            // We are only storing in this unordered map data binds that are
            // targetting the source. For now, this is only the case for
            // listener actions.
            if (static_cast<DataBindFlags>(dataBindClone->flags()) ==
                DataBindFlags::ToSource)
            {
                m_bindableDataBindsToSource[bindablePropertyClone] =
                    dataBindClone;
            }
            else
            {
                m_bindableDataBindsToTarget[bindablePropertyClone] =
                    dataBindClone;
            }
        }
        else
        {
            dataBindClone->target(dataBind->target());
        }
    }

    // Initialize listeners. Store a lookup table of shape id to hit shape
    // representation (an object that stores all the listeners triggered by the
    // shape producing a listener).
    std::unordered_map<Component*, HitDrawable*> hitLookup;
    for (std::size_t i = 0; i < machine->listenerCount(); i++)
    {
        auto listener = machine->listener(i);
        if (listener->listenerType() == ListenerType::event)
        {
            continue;
        }
        auto listenerGroup = rivestd::make_unique<ListenerGroup>(listener);
        auto target = m_artboardInstance->resolve(listener->targetId());
        if (target != nullptr && target->is<Component>())
        {
            bool isLayoutComponent = false;
            if (target->is<LayoutComponent>())
            {
                isLayoutComponent = true;
                target = target->as<LayoutComponent>()->proxy();
            }
            addToHitLookup(target->as<Component>(),
                           isLayoutComponent,
                           hitLookup,
                           listenerGroup.get(),
                           false);
        }
        m_listenerGroups.push_back(std::move(listenerGroup));
    }

    std::vector<DraggableConstraint*> draggableConstraints;
    for (auto core : m_artboardInstance->objects())
    {
        if (core == nullptr)
        {
            continue;
        }
        if (core->is<DraggableConstraint>())
        {
            draggableConstraints.push_back(core->as<DraggableConstraint>());
        }
    }
    for (auto constraint : draggableConstraints)
    {
        auto draggables = constraint->draggables();
        for (auto dragProxy : draggables)
        {
            auto listener = new StateMachineListener();
            listener->listenerTypeValue(
                static_cast<uint32_t>(ListenerType::draggableConstraint));
            auto listenerGroup =
                rivestd::make_unique<DraggableConstraintListenerGroup>(
                    listener,
                    constraint,
                    dragProxy);
            auto hittable = dragProxy->hittable();
            if (hittable != nullptr && hittable->is<Component>())
            {
                addToHitLookup(hittable->as<Component>(),
                               hittable->is<LayoutComponent>() ||
                                   hittable->isProxy(),
                               hitLookup,
                               listenerGroup.get(),
                               dragProxy->isOpaque());
            }
            m_listenerGroups.push_back(std::move(listenerGroup));
        }
    }

    for (auto nestedArtboard : instance->nestedArtboards())
    {
        if (nestedArtboard->hasNestedStateMachines())
        {
            auto hn = rivestd::make_unique<HitNestedArtboard>(
                nestedArtboard->as<Component>(),
                this);
            m_hitComponents.push_back(std::move(hn));
        }
        for (auto animation : nestedArtboard->nestedAnimations())
        {
            if (animation->is<NestedStateMachine>())
            {
                if (auto notifier = animation->as<NestedStateMachine>()
                                        ->stateMachineInstance())
                {
                    notifier->setNestedArtboard(nestedArtboard);
                    notifier->addNestedEventListener(this);
                }
            }
            else if (animation->is<NestedLinearAnimation>())
            {
                if (auto notifier = animation->as<NestedLinearAnimation>()
                                        ->animationInstance())
                {
                    notifier->setNestedArtboard(nestedArtboard);
                    notifier->addNestedEventListener(this);
                }
            }
        }
    }
    for (auto componentList : instance->artboardComponentLists())
    {
        auto hc = rivestd::make_unique<HitComponentList>(
            componentList->as<Component>(),
            this);
        m_hitComponents.push_back(std::move(hc));
    }
    sortHitComponents();
}

StateMachineInstance::~StateMachineInstance()
{
    unbind();
    for (auto inst : m_inputInstances)
    {
        delete inst;
    }
    for (auto& listenerGroup : m_listenerGroups)
    {
        listenerGroup.reset();
    }
    for (auto databind : m_dataBinds)
    {
        delete databind;
    }
    delete[] m_layers;
    for (auto pair : m_bindablePropertyInstances)
    {
        delete pair.second;
        pair.second = nullptr;
    }
    if (m_ownsDataContext)
    {
        delete m_DataContext;
    }
    m_bindablePropertyInstances.clear();
}

void StateMachineInstance::removeEventListeners()
{
    if (m_artboardInstance != nullptr)
    {
        for (auto nestedArtboard : m_artboardInstance->nestedArtboards())
        {
            if (nestedArtboard == nullptr)
            {
                continue;
            }
            for (auto animation : nestedArtboard->nestedAnimations())
            {
                if (animation == nullptr)
                {
                    continue;
                }
                if (animation->is<NestedStateMachine>())
                {
                    if (auto notifier = animation->as<NestedStateMachine>()
                                            ->stateMachineInstance())
                    {
                        notifier->removeNestedEventListener(this);
                    }
                }
                else if (animation->is<NestedLinearAnimation>())
                {
                    if (auto notifier = animation->as<NestedLinearAnimation>()
                                            ->animationInstance())
                    {
                        notifier->removeNestedEventListener(this);
                    }
                }
            }
        }
    }
}

#ifdef WITH_RIVE_TOOLS
void StateMachineInstance::onDataBindChanged(DataBindChanged callback)
{
    for (auto databind : m_dataBinds)
    {
        databind->onChanged(callback);
    }
}
#endif

void StateMachineInstance::sortHitComponents()
{
    auto hitShapesCount = m_hitComponents.size();
    auto currentSortedIndex = 0;
    auto count = 0;
    // Since the Artboard is not a drawable, we move all hit components
    // pointing to the artboard to the front of the list
    for (auto& comp : m_hitComponents)
    {
        if (comp->component() != nullptr && comp->component()->is<Artboard>())
        {
            if (currentSortedIndex != count)
            {

                std::iter_swap(m_hitComponents.begin() + currentSortedIndex,
                               m_hitComponents.begin() + count);
            }
            currentSortedIndex++;
        }
        count++;
    }
    Drawable* last = m_artboardInstance->firstDrawable();
    if (last)
    {
        // walk to the end, so we can visit in reverse-order
        while (last->prev)
        {
            last = last->prev;
        }
    }
    for (auto drawable = last; drawable; drawable = drawable->next)
    {
        for (size_t i = currentSortedIndex; i < hitShapesCount; i++)
        {
            if (m_hitComponents[i]->component() == drawable)
            {
                if (currentSortedIndex != i)
                {
                    std::iter_swap(m_hitComponents.begin() + currentSortedIndex,
                                   m_hitComponents.begin() + i);
                }
                currentSortedIndex++;
            }
        }
        if (currentSortedIndex == hitShapesCount)
        {
            break;
        }
    }
}

void StateMachineInstance::updateDataBinds()
{
    for (auto dataBind : m_dataBinds)
    {
        auto d = dataBind->dirt();
        if (d != ComponentDirt::None)
        {
            dataBind->dirt(ComponentDirt::None);
            dataBind->update(d);
        }
    }
}

bool StateMachineInstance::tryChangeState()
{
    updateDataBinds();
    bool hasChangedState = false;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].updateState())
        {
            hasChangedState = true;
        }
    }
    return hasChangedState;
}

bool StateMachineInstance::advance(float seconds, bool newFrame)
{
    if (m_drawOrderChangeCounter !=
        m_artboardInstance->drawOrderChangeCounter())
    {
        m_drawOrderChangeCounter = m_artboardInstance->drawOrderChangeCounter();
        sortHitComponents();
    }
    if (newFrame)
    {
        this->notifyEventListeners(m_reportedEvents, nullptr);
        m_reportedEvents.clear();
        m_needsAdvance = false;
    }
    updateDataBinds();
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].advance(seconds, newFrame))
        {
            m_needsAdvance = true;
        }
    }

    for (auto& dataBind : m_dataBinds)
    {
        if (dataBind->advance(seconds))
        {
            m_needsAdvance = true;
        }
    }

    for (auto inst : m_inputInstances)
    {
        inst->advanced();
    }

    return m_needsAdvance || !m_reportedEvents.empty();
}

void StateMachineInstance::advancedDataContext()
{
    if (m_DataContext != nullptr)
    {
        m_DataContext->advanced();
    }
}

bool StateMachineInstance::advanceAndApply(float seconds)
{
    bool keepGoing = this->advance(seconds, true);
    if (m_artboardInstance->advanceInternal(
            seconds,
            AdvanceFlags::IsRoot | AdvanceFlags::Animate |
                AdvanceFlags::AdvanceNested | AdvanceFlags::NewFrame))
    {
        keepGoing = true;
    }

    for (int outerOptionC = 0; outerOptionC < 5; outerOptionC++)
    {
        if (m_artboardInstance->updatePass(true))
        {
            keepGoing = true;
        }

        // Advance all animations.
        if (this->tryChangeState())
        {
            this->advance(0.0f, false);
            keepGoing = true;
        }

        if (m_artboardInstance->advanceInternal(
                0.0f,
                AdvanceFlags::IsRoot | AdvanceFlags::Animate |
                    AdvanceFlags::AdvanceNested))
        {
            keepGoing = true;
        }
        advancedDataContext();

        if (!m_artboardInstance->hasDirt(ComponentDirt::Components))
        {
            break;
        }
    }
    return keepGoing || !m_reportedEvents.empty();
}

void StateMachineInstance::markNeedsAdvance() { m_needsAdvance = true; }
bool StateMachineInstance::needsAdvance() const { return m_needsAdvance; }

void StateMachineInstance::resetState()
{
    for (size_t i = 0; i < m_layerCount; i++)
    {
        m_layers[i].resetState();
    }
}

std::string StateMachineInstance::name() const { return m_machine->name(); }

SMIInput* StateMachineInstance::input(size_t index) const
{
    if (index < m_inputInstances.size())
    {
        return m_inputInstances[index];
    }
    return nullptr;
}

template <typename SMType, typename InstType>
InstType* StateMachineInstance::getNamedInput(const std::string& name) const
{
    for (const auto inst : m_inputInstances)
    {
        auto input = inst->input();
        if (input->is<SMType>() && input->name() == name)
        {
            return static_cast<InstType*>(inst);
        }
    }
    return nullptr;
}

SMIBool* StateMachineInstance::getBool(const std::string& name) const
{
    return getNamedInput<StateMachineBool, SMIBool>(name);
}
SMINumber* StateMachineInstance::getNumber(const std::string& name) const
{
    return getNamedInput<StateMachineNumber, SMINumber>(name);
}
SMITrigger* StateMachineInstance::getTrigger(const std::string& name) const
{
    return getNamedInput<StateMachineTrigger, SMITrigger>(name);
}

void StateMachineInstance::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance)
{
    clearDataContext();
    m_ownsDataContext = true;
    auto dataContext = new DataContext(viewModelInstance);
    m_artboardInstance->clearDataContext();
    m_artboardInstance->internalDataContext(dataContext);
    internalDataContext(dataContext);
}

void StateMachineInstance::dataContext(DataContext* dataContext)
{
    clearDataContext();
    internalDataContext(dataContext);
}

void StateMachineInstance::internalDataContext(DataContext* dataContext)
{
    m_DataContext = dataContext;
    for (auto dataBind : m_dataBinds)
    {
        if (dataBind->is<DataBindContext>())
        {
            dataBind->as<DataBindContext>()->bindFromContext(dataContext);
        }
    }
}

void StateMachineInstance::clearDataContext()
{
    if (m_ownsDataContext && m_DataContext != nullptr)
    {
        delete m_DataContext;
        m_DataContext = nullptr;
    }

    m_ownsDataContext = false;
}

void StateMachineInstance::unbind()
{
    clearDataContext();
    for (auto dataBind : m_dataBinds)
    {
        dataBind->unbind();
    }
}

size_t StateMachineInstance::stateChangedCount() const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].stateChangedOnAdvance())
        {
            count++;
        }
    }
    return count;
}

const LayerState* StateMachineInstance::stateChangedByIndex(size_t index) const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].stateChangedOnAdvance())
        {
            if (count == index)
            {
                return m_layers[i].currentState();
            }
            count++;
        }
    }
    return nullptr;
}

size_t StateMachineInstance::currentAnimationCount() const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].currentAnimation() != nullptr)
        {
            count++;
        }
    }
    return count;
}

const LinearAnimationInstance* StateMachineInstance::currentAnimationByIndex(
    size_t index) const
{
    size_t count = 0;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].currentAnimation() != nullptr)
        {
            if (count == index)
            {
                return m_layers[i].currentAnimation();
            }
            count++;
        }
    }
    return nullptr;
}

void StateMachineInstance::reportEvent(Event* event, float delaySeconds)
{
    m_reportedEvents.push_back(EventReport(event, delaySeconds));
}

std::size_t StateMachineInstance::reportedEventCount() const
{
    return m_reportedEvents.size();
}

const EventReport StateMachineInstance::reportedEventAt(std::size_t index) const
{
    if (index >= m_reportedEvents.size())
    {
        return EventReport(nullptr, 0.0f);
    }
    return m_reportedEvents[index];
}

void StateMachineInstance::notify(const std::vector<EventReport>& events,
                                  NestedArtboard* context)
{
    notifyEventListeners(events, context);
    updateDataBinds();
}

void StateMachineInstance::notifyEventListeners(
    const std::vector<EventReport>& events,
    NestedArtboard* source)
{
    if (events.size() > 0)
    {
        // We trigger the listeners in order
        for (size_t i = 0; i < m_machine->listenerCount(); i++)
        {
            auto listener = m_machine->listener(i);
            auto target = artboard()->resolve(listener->targetId());
            if (listener != nullptr &&
                listener->listenerType() == ListenerType::event &&
                (source == nullptr || source == target))
            {
                for (const auto event : events)
                {
                    auto sourceArtboard = source == nullptr
                                              ? artboard()
                                              : source->artboardInstance();

                    // listener->eventId() can point to an id from an
                    // event in the context of this artboard or the
                    // context of a nested artboard. Because those ids
                    // belong to different contexts, they can have the
                    // same value. So when the eventId is resolved
                    // within one context, but actually pointing to the
                    // other, it can return the wrong event object. If,
                    // by chance, that event exists in the other
                    // context, and is being reported, it will trigger
                    // the wrong set of actions. This validation makes
                    // sure that a listener must be targetting the
                    // current artboard to disambiguate between external
                    // and internal events.
                    if (source == nullptr &&
                        sourceArtboard->resolve(listener->targetId()) !=
                            artboard())
                    {
                        continue;
                    }
                    auto listenerEvent =
                        sourceArtboard->resolve(listener->eventId());
                    if (listenerEvent == event.event())
                    {
                        listener->performChanges(this, Vec2D(), Vec2D());
                        break;
                    }
                }
            }
        }
        // Bubble the event up to parent artboard state machines
        // immediately
        for (auto listener : nestedEventListeners())
        {
            listener->notify(events, nestedArtboard());
        }

        for (auto report : events)
        {
            auto event = report.event();
            if (event->is<AudioEvent>())
            {
                event->as<AudioEvent>()->play();
            }
        }
    }
}

BindableProperty* StateMachineInstance::bindablePropertyInstance(
    BindableProperty* bindableProperty) const
{
    auto bindablePropertyInstance =
        m_bindablePropertyInstances.find(bindableProperty);
    if (bindablePropertyInstance == m_bindablePropertyInstances.end())
    {
        return nullptr;
    }
    return bindablePropertyInstance->second;
}

DataBind* StateMachineInstance::bindableDataBindToSource(
    BindableProperty* bindableProperty) const
{
    auto dataBind = m_bindableDataBindsToSource.find(bindableProperty);
    if (dataBind == m_bindableDataBindsToSource.end())
    {
        return nullptr;
    }
    return dataBind->second;
}

DataBind* StateMachineInstance::bindableDataBindToTarget(
    BindableProperty* bindableProperty) const
{
    auto dataBind = m_bindableDataBindsToTarget.find(bindableProperty);
    if (dataBind == m_bindableDataBindsToTarget.end())
    {
        return nullptr;
    }
    return dataBind->second;
}
