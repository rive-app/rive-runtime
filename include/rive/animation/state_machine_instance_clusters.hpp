#ifndef _RIVE_STATE_MACHINE_INSTANCE_CLUSTERS_HPP_
#define _RIVE_STATE_MACHINE_INSTANCE_CLUSTERS_HPP_

// Implementation detail of StateMachineInstance. Only the translation units
// that define StateMachineInstance members need it — state_machine_instance.cpp
// and src/input/gamepad_batch.cpp — and state_machine_instance.hpp forward
// declares these types rather than including this, so its heavier includes
// (listener groups, SemanticManager, GamepadSnapshot) stay out of the public
// surface. Do not include it just to reach a state machine; everything callers
// need is on StateMachineInstance itself.
//
// StateMachineInstance is built once per row of an ArtboardComponentList, so
// every inline byte is multiplied by the row count. The clusters below hold the
// state that is untouched on a plain state machine — events, bindable-property
// instancing, focus / keyboard / gamepad / semantics, and scripting — and hang
// off the instance through a `Sidecar` (8 B, null until the feature is
// actually used). Read paths null-check the sidecar, which is strictly cheaper
// than the per-container `.empty()` probes it replaces; write paths call the
// matching `ensure...()` on StateMachineInstance.

#include "rive/animation/gamepad_listener_group.hpp"
#include "rive/animation/keyboard_listener_group.hpp"
#include "rive/animation/semantic_listener_group.hpp"
#include "rive/event_report.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive/semantic/semantic_manager.hpp"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rive
{
class BindableProperty;
class BindablePropertyNumber;
class Core;
class DataBind;
class FocusListenerGroup;
class ListenerViewModel;
class ScriptedDrawable;
class ScriptedObject;

/// Event and viewModel-listener reporting. Allocated the first time the file
/// reports an event or registers a viewModel listener.
struct SMIReporting
{
    /// Events reported since the last applyEvents, awaiting delivery.
    std::vector<EventReport> reportedEvents;
    /// The batch currently being delivered. The reported/reporting split is
    /// load-bearing for re-entrancy, not just capacity caching: a listener
    /// action can re-enter reportEvent() while this batch is being notified,
    /// and those new events must land in `reportedEvents` for the next pass.
    std::vector<EventReport> reportingEvents;
    /// Events first reported inside the applyEvents loop; kept host visible
    /// until the next applyEvents so hosts see them exactly once.
    std::vector<EventReport> eventsAppliedDuringLoop;
    /// Owned, one per viewModel listener declared by the state machine.
    std::vector<ListenerViewModel*> listenerViewModels;
    std::vector<ListenerViewModel*> reportedListenerViewModels;
    std::vector<ListenerViewModel*> reportingListenerViewModels;
};

/// Per-instance clones of the BindableProperty / StateTransition values a data
/// bind writes to, so instances never write through to shared file data.
/// Allocated only when a state machine data bind targets one of them.
struct SMIBindables
{
    /// Shared BindableProperty -> this instance's owned clone.
    std::unordered_map<BindableProperty*, BindableProperty*> propertyInstances;
    std::unordered_map<BindableProperty*, DataBind*> dataBindsToTarget;
    std::unordered_map<BindableProperty*, DataBind*> dataBindsToSource;
    /// Map from shared StateTransition* to per-instance BindablePropertyNumber
    /// instances, keyed by original property key. Data binds write to these
    /// instead of the shared StateTransition object.
    std::unordered_map<const Core*,
                       std::unordered_map<uint32_t, BindablePropertyNumber*>>
        transitionPropertyInstances;
};

struct QueuedFocusEvent
{
    FocusListenerGroup* group;
    bool isFocus;
};

struct QueuedSemanticEvent
{
    SemanticListenerGroup* group;
    SemanticActionType actionType;
};

/// Everything driven by focus, keyboard, gamepad, or accessibility rather than
/// by pointer events. Allocated when the file declares a focus/blur, keyboard,
/// textInput, gamepad, or semanticAction listener, when a script wants keyboard
/// or gamepad input, or when enableSemantics() is called.
struct SMIInputExtras
{
    std::vector<std::unique_ptr<FocusListenerGroup>> focusListenerGroups;
    std::vector<std::unique_ptr<KeyboardListenerGroup>> keyboardListenerGroups;
    std::vector<std::unique_ptr<GamepadListenerGroup>> gamepadListenerGroups;
    std::vector<std::unique_ptr<SemanticListenerGroup>> semanticListenerGroups;
    /// Non-owning back-references to every `ScriptedDrawable` whose script
    /// declares a gamepad handler. Populated once at init from the artboard
    /// (which outlives this state machine) and walked by
    /// `broadcastGamepadToScriptedDrawables` so events reach scripts that are
    /// not on the focus chain. Mirrors how `m_hitComponents` lets every
    /// pointer-aware drawable react regardless of focus.
    std::vector<ScriptedDrawable*> gamepadScriptedDrawables;
    /// Latest embedder gamepad state for `submitGamepadsFromBuffer` (WASM/JS).
    std::unordered_map<int, GamepadSnapshot> embedderGamepads;

    std::unique_ptr<SemanticManager> semanticManager;
    SemanticManager* externalSemanticManager = nullptr;

    /// Queued for deferred execution during advance().
    std::vector<QueuedFocusEvent> queuedFocusEvents;
    std::vector<QueuedSemanticEvent> queuedSemanticEvents;
};

/// Per-instance clones of the state machine's scripted objects. Allocated only
/// when the file uses scripting.
struct SMIScripting
{
    /// (shared source, owned instance) pairs in the state machine's authored
    /// order. A vector rather than a map so Lua `init` runs in a deterministic
    /// order; lookups are a handful of entries and are always followed by a VM
    /// call that dwarfs the scan.
    std::vector<std::pair<const ScriptedObject*, ScriptedObject*>> objects;

    ScriptedObject* find(const ScriptedObject* source) const
    {
        for (const auto& pair : objects)
        {
            if (pair.first == source)
            {
                return pair.second;
            }
        }
        return nullptr;
    }
};

} // namespace rive
#endif
