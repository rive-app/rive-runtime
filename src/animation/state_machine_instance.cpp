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
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch())
                         .count();
        srand(nanos);
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

    bool advance(float seconds)
    {
        m_stateMachineChangedOnAdvance = false;
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

        for (int i = 0; updateState(i != 0); i++)
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

    bool updateState(bool ignoreTriggers)
    {
        // Don't allow changing state while a transition is taking place
        // (we're mixing one state onto another) if enableEarlyExit is not true.
        if (isTransitioning() && !m_transition->enableEarlyExit())
        {
            return false;
        }

        m_waitingForExit = false;

        if (tryChangeState(m_anyStateInstance, ignoreTriggers))
        {
            return true;
        }

        return tryChangeState(m_currentState, ignoreTriggers);
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

    double randomValue() { return ((double)rand() / (RAND_MAX)); }

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

    StateTransition* findRandomTransition(StateInstance* stateFromInstance,
                                          bool ignoreTriggers)
    {
        uint32_t totalWeight = 0;
        auto stateFrom = stateFromInstance->state();
        for (size_t i = 0, length = stateFrom->transitionCount(); i < length;
             i++)
        {
            auto transition = stateFrom->transition(i);
            auto allowed = transition->allowed(stateFromInstance,
                                               m_stateMachineInstance,
                                               ignoreTriggers);
            if (allowed == AllowTransition::yes &&
                canChangeState(transition->stateTo()))
            {
                transition->evaluatedRandomWeight(transition->randomWeight());
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
        if (totalWeight > 0)
        {
            double randomWeight = randomValue() * totalWeight * 1.0;
            float currentWeight = 0;
            size_t index = 0;
            StateTransition* transition;
            while (index < stateFrom->transitionCount())
            {
                transition = stateFrom->transition(index);
                auto transitionWeight = transition->evaluatedRandomWeight();
                if (currentWeight + transitionWeight > randomWeight)
                {
                    return transition;
                }
                currentWeight += transitionWeight;
                index++;
            }
        }
        return nullptr;
    }

    StateTransition* findAllowedTransition(StateInstance* stateFromInstance,
                                           bool ignoreTriggers)
    {
        auto stateFrom = stateFromInstance->state();
        // If it should randomize
        if ((static_cast<LayerStateFlags>(stateFrom->flags()) &
             LayerStateFlags::Random) == LayerStateFlags::Random)
        {
            return findRandomTransition(stateFromInstance, ignoreTriggers);
        }
        // Else search the first valid transition
        for (size_t i = 0, length = stateFrom->transitionCount(); i < length;
             i++)
        {
            auto transition = stateFrom->transition(i);
            auto allowed = transition->allowed(stateFromInstance,
                                               m_stateMachineInstance,
                                               ignoreTriggers);
            if (allowed == AllowTransition::yes &&
                canChangeState(transition->stateTo()))
            {
                transition->evaluatedRandomWeight(transition->randomWeight());
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

    bool tryChangeState(StateInstance* stateFromInstance, bool ignoreTriggers)
    {
        if (stateFromInstance == nullptr)
        {
            return false;
        }
        auto outState = m_currentState;
        auto transition =
            findAllowedTransition(stateFromInstance, ignoreTriggers);
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
    virtual ~DraggableConstraintListenerGroup() {}

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
        StateMachineInstance* stateMachineInstance) override
    {
        auto prevPhase = clickPhase();
        ListenerGroup::processEvent(component,
                                    position,
                                    hitEvent,
                                    canHit,
                                    stateMachineInstance);
        if (prevPhase == GestureClickPhase::down &&
            (clickPhase() == GestureClickPhase::clicked ||
             clickPhase() == GestureClickPhase::out))
        {
            m_draggable->endDrag(position);
            if (hasScrolled)
            {
                return ProcessEventResult::scroll;
                hasScrolled = false;
            }
        }
        else if (prevPhase != GestureClickPhase::down &&
                 clickPhase() == GestureClickPhase::down)
        {
            m_draggable->startDrag(position);
            hasScrolled = false;
        }
        else if (hitEvent == ListenerType::move &&
                 clickPhase() == GestureClickPhase::down)
        {
            m_draggable->drag(position);
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

class HitDrawable : public HitComponent
{
public:
    HitDrawable(Component* component,
                StateMachineInstance* stateMachineInstance,
                bool isOpaque) :
        HitComponent(component, stateMachineInstance)
    {
        this->isOpaque = isOpaque;
        if (component->as<Drawable>()->isTargetOpaque())
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
    std::vector<ListenerGroup*> listeners;

    virtual bool hitTestHelper(Vec2D position) const { return false; }

    bool hitTest(Vec2D position) const override
    {
        return hitTestHelper(position);
    }

    virtual bool testBounds(ContainerComponent* component,
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
        }
        if (component == nullptr || !component->is<LayoutComponent>())
        {
            return true;
        }
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
                    if (artboard->frameOrigin())
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
                           bool canHit) override
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
        auto drawable = m_component->as<Drawable>();
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
                                            m_stateMachineInstance) ==
                ProcessEventResult::scroll)
            {
                isBlockingEvent = true;
            }
        }
        return (isHovered && canHit)
                   ? (isOpaque || drawable->isTargetOpaque() || isBlockingEvent)
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

/// Representation of a Shape from the Artboard Instance and all the listeners
/// it triggers. Allows tracking hover and performing hit detection only once on
/// shapes that trigger multiple listeners.
class HitShape : public HitDrawable
{
public:
    bool testBounds(ContainerComponent* component,
                    Vec2D position,
                    bool skipOnUnclipped) const override
    {
        if (component != nullptr && component->is<Shape>())
        {
            auto shape = component->as<Shape>();
            auto worldBounds = shape->worldBounds();
            if (worldBounds.contains(position) &&
                HitDrawable::testBounds(component->parent(), position, true))
            {
                auto hitArea = AABB(position.x - hitRadius,
                                    position.y - hitRadius,
                                    position.x + hitRadius,
                                    position.y + hitRadius)
                                   .round();
                return shape->hitTest(hitArea);
            }
        }
        else
        {
            return HitDrawable::testBounds(component, position, true);
        }
        return false;
    }

    HitShape(Component* shape,
             StateMachineInstance* stateMachineInstance,
             bool isOpaque = false) :
        HitDrawable(shape, stateMachineInstance, isOpaque)
    {}

    bool hitTestHelper(Vec2D position) const override
    {
        return testBounds(m_component->as<Shape>(), position, true);
    }
};

class HitLayout : public HitDrawable
{
public:
    HitLayout(Component* layout,
              StateMachineInstance* stateMachineInstance,
              bool isOpaque = false) :
        HitDrawable(layout, stateMachineInstance, isOpaque)
    {}

    bool hitTestHelper(Vec2D position) const override
    {
        return testBounds(m_component->as<Drawable>(), position, false);
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
                           bool canHit) override
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
                                nestedStateMachine->pointerMove(nestedPosition);
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

} // namespace rive

HitResult StateMachineInstance::updateListeners(Vec2D position,
                                                ListenerType hitType)
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
            hitShape->processEvent(position, hitType, !hitOpaque);
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

HitResult StateMachineInstance::pointerMove(Vec2D position)
{
    return updateListeners(position, ListenerType::move);
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
                m_bindableDataBinds[bindablePropertyClone] = dataBindClone;
            }
        }
    }

    // Initialize listeners. Store a lookup table of shape id to hit shape
    // representation (an object that stores all the listeners triggered by the
    // shape producing a listener).
    std::unordered_map<Component*, HitDrawable*> hitShapeLookup;
    for (std::size_t i = 0; i < machine->listenerCount(); i++)
    {
        auto listener = machine->listener(i);
        if (listener->listenerType() == ListenerType::event)
        {
            continue;
        }
        auto listenerGroup = rivestd::make_unique<ListenerGroup>(listener);
        auto target = m_artboardInstance->resolve(listener->targetId());
        if (target != nullptr && target->is<ContainerComponent>())
        {
            if (target->is<LayoutComponent>())
            {
                auto component = target->as<LayoutComponent>()->proxy();
                HitLayout* hitLayout;
                auto itr = hitShapeLookup.find(component);
                if (itr == hitShapeLookup.end())
                {
                    auto hs = rivestd::make_unique<HitLayout>(component, this);
                    hitShapeLookup[component] = hitLayout = hs.get();
                    m_hitComponents.push_back(std::move(hs));
                }
                else
                {
                    hitLayout = static_cast<HitLayout*>(itr->second);
                }
                hitLayout->addListener(listenerGroup.get());
            }
            else
            {

                target->as<ContainerComponent>()->forAll(
                    [&](Component* component) {
                        if (component->is<Shape>())
                        {
                            HitShape* hitShape;
                            auto itr = hitShapeLookup.find(component);
                            if (itr == hitShapeLookup.end())
                            {
                                component->as<Shape>()->addFlags(
                                    PathFlags::neverDeferUpdate);
                                component->as<Shape>()->addDirt(
                                    ComponentDirt::Path,
                                    true);
                                auto hs =
                                    rivestd::make_unique<HitShape>(component,
                                                                   this);
                                hitShapeLookup[component] = hitShape = hs.get();
                                m_hitComponents.push_back(std::move(hs));
                            }
                            else
                            {
                                hitShape = static_cast<HitShape*>(itr->second);
                            }
                            hitShape->addListener(listenerGroup.get());
                        }
                        return true;
                    });
            }
        }
        m_listenerGroups.push_back(std::move(listenerGroup));
    }

    std::unordered_map<Component*, HitDrawable*> hitScrollLookup;
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
            if (hittable == nullptr)
            {
                continue;
            }
            if (hittable->is<LayoutComponent>() || hittable->isProxy())
            {
                auto component = hittable->as<Drawable>();
                HitLayout* hitLayout;
                auto itr = hitScrollLookup.find(component);
                if (itr == hitScrollLookup.end())
                {
                    auto hs =
                        rivestd::make_unique<HitLayout>(component,
                                                        this,
                                                        dragProxy->isOpaque());
                    hitScrollLookup[component] = hitLayout = hs.get();
                    m_hitComponents.push_back(std::move(hs));
                }
                else
                {
                    hitLayout = static_cast<HitLayout*>(itr->second);
                }
                hitLayout->addListener(listenerGroup.get());
            }
            else
            {
                hittable->as<ContainerComponent>()->forAll(
                    [&](Component* component) {
                        if (component->is<Shape>())
                        {
                            HitShape* hitShape;
                            auto itr = hitScrollLookup.find(component);
                            if (itr == hitScrollLookup.end())
                            {
                                component->as<Shape>()->addFlags(
                                    PathFlags::neverDeferUpdate);
                                component->as<Shape>()->addDirt(
                                    ComponentDirt::Path,
                                    true);
                                auto hs = rivestd::make_unique<HitShape>(
                                    component,
                                    this,
                                    dragProxy->isOpaque());
                                hitScrollLookup[component] = hitShape =
                                    hs.get();
                                m_hitComponents.push_back(std::move(hs));
                            }
                            else
                            {
                                hitShape = static_cast<HitShape*>(itr->second);
                            }
                            hitShape->addListener(listenerGroup.get());
                        }
                        return true;
                    });
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
                auto notifier =
                    animation->as<NestedStateMachine>()->stateMachineInstance();
                notifier->setNestedArtboard(nestedArtboard);
                notifier->addNestedEventListener(this);
            }
            else if (animation->is<NestedLinearAnimation>())
            {
                auto notifier =
                    animation->as<NestedLinearAnimation>()->animationInstance();
                notifier->setNestedArtboard(nestedArtboard);
                notifier->addNestedEventListener(this);
            }
        }
    }
    sortHitComponents();
}

StateMachineInstance::~StateMachineInstance()
{
    for (auto inst : m_inputInstances)
    {
        delete inst;
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
    m_bindablePropertyInstances.clear();
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
    // Since the Artboard is not a drawable, we move all hit components pointing
    // to the artboard to the front of the list
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
    bool hasChangedState = false;
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].updateState(true))
        {
            hasChangedState = true;
        }
    }
    return hasChangedState;
}

bool StateMachineInstance::advance(float seconds, bool newFrame)
{
    updateDataBinds();
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
    for (size_t i = 0; i < m_layerCount; i++)
    {
        if (m_layers[i].advance(seconds))
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
        m_DataContext->viewModelInstance()->advanced();
    }
}

bool StateMachineInstance::advanceAndApply(float seconds)
{
    advancedDataContext();
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

        if (!m_artboardInstance->hasDirt(ComponentDirt::Components))
        {
            break;
        }
    }
    return keepGoing || !m_reportedEvents.empty();
}

void StateMachineInstance::markNeedsAdvance() { m_needsAdvance = true; }
bool StateMachineInstance::needsAdvance() const { return m_needsAdvance; }

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

void StateMachineInstance::setDataContextFromInstance(
    ViewModelInstance* viewModelInstance)
{
    dataContext(new DataContext(viewModelInstance));
}

void StateMachineInstance::dataContext(DataContext* dataContext)
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

                    // listener->eventId() can point to an id from an event in
                    // the context of this artboard or the context of a nested
                    // artboard. Because those ids belong to different contexts,
                    // they can have the same value. So when the eventId is
                    // resolved within one context, but actually pointing to the
                    // other, it can return the wrong event object. If, by
                    // chance, that event exists in the other context, and is
                    // being reported, it will trigger the wrong set of actions.
                    // This validation makes sure that a listener must be
                    // targetting the current artboard to disambiguate between
                    // external and internal events.
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
        // Bubble the event up to parent artboard state machines immediately
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

DataBind* StateMachineInstance::bindableDataBind(
    BindableProperty* bindableProperty)
{
    auto dataBind = m_bindableDataBinds.find(bindableProperty);
    if (dataBind == m_bindableDataBinds.end())
    {
        return nullptr;
    }
    return dataBind->second;
}
