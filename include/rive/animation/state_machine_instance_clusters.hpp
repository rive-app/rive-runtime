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

#include <cstdint>
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

/// Shared BindableProperty -> an instance's clone of it. Transition conditions
/// and blend states look their clone up every time they are evaluated, so this
/// is a flat open-addressing table keyed by the pointer: one multiply to hash,
/// and the entry sits in a contiguous array, where an unordered_map also
/// divides and chases a separately allocated node for every lookup.
class BindablePropertyInstances
{
public:
    BindableProperty* find(const BindableProperty* shared) const
    {
        if (m_entries.empty())
        {
            return nullptr;
        }
        const size_t mask = m_entries.size() - 1;
        for (size_t i = slotOf(shared) & mask;; i = (i + 1) & mask)
        {
            const Entry& entry = m_entries[i];
            if (entry.shared == shared)
            {
                return entry.instance;
            }
            if (entry.shared == nullptr)
            {
                return nullptr;
            }
        }
    }

    /// Adds [shared] -> [instance]; [shared] must not be in the table yet.
    void insert(BindableProperty* shared, BindableProperty* instance)
    {
        // Keep at least half the entries empty so probes stay short.
        if ((m_count + 1) * 2 > m_entries.size())
        {
            grow();
        }
        place(shared, instance);
        m_count++;
    }

    template <typename F> void forEachInstance(F&& visit) const
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.shared != nullptr)
            {
                visit(entry.instance);
            }
        }
    }

    void clear()
    {
        m_entries.clear();
        m_count = 0;
    }

private:
    struct Entry
    {
        BindableProperty* shared = nullptr;
        BindableProperty* instance = nullptr;
    };

    // Fibonacci hashing: the multiply spreads the pointer's bits, so
    // neighbouring allocations still land far apart.
    static size_t slotOf(const BindableProperty* shared)
    {
        return (size_t)(((uint64_t)(uintptr_t)shared *
                         UINT64_C(0x9E3779B97F4A7C15)) >>
                        32);
    }

    void place(BindableProperty* shared, BindableProperty* instance)
    {
        const size_t mask = m_entries.size() - 1;
        size_t i = slotOf(shared) & mask;
        while (m_entries[i].shared != nullptr)
        {
            i = (i + 1) & mask;
        }
        m_entries[i] = {shared, instance};
    }

    void grow()
    {
        std::vector<Entry> old = std::move(m_entries);
        m_entries.assign(old.empty() ? 8 : old.size() * 2, Entry{});
        for (const Entry& entry : old)
        {
            if (entry.shared != nullptr)
            {
                place(entry.shared, entry.instance);
            }
        }
    }

    // Power-of-two size; a null shared pointer marks an empty entry.
    std::vector<Entry> m_entries;
    size_t m_count = 0;
};

/// Per-instance clones of the BindableProperty / StateTransition values a data
/// bind writes to, so instances never write through to shared file data.
/// Allocated only when a state machine data bind targets one of them.
struct SMIBindables
{
    /// Shared BindableProperty -> this instance's owned clone.
    BindablePropertyInstances propertyInstances;
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
