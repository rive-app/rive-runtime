#ifndef _RIVE_DATA_CONTEXT_HPP_
#define _RIVE_DATA_CONTEXT_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/data_resolver.hpp"
#include "rive/refcnt.hpp"
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace rive
{
class DataBindPath;
class DataContext : public RefCnt<DataContext>
{
private:
    // Sentinel slot key for an entry that is not on the slot keys, i.e. a main
    // view model instance. A real slot key is a viewModelId (an index into the
    // file's view models), so this value can never be a valid slot.
    static constexpr uint32_t kNoSlot = std::numeric_limits<uint32_t>::max();

    rcp<DataContext> m_Parent = nullptr;
    std::vector<rcp<ViewModelInstance>> m_ViewModelInstances;

    // Containers (artboard + its state machines) that share this context and
    // must be kept registered as dependents of every instance it holds. The
    // context owns this bookkeeping so that mutating a shared context re-points
    // ALL attached containers, not just the caller. Raw pointers: a container
    // deregisters via removeDependentContainer before it dies (and the
    // destructor detaches defensively).
    std::vector<DataBindContainer*> m_dependentContainers;
    // Add/remove every attached container as a dependent of one instance.
    void attachContainers(ViewModelInstance* instance);
    void detachContainers(ViewModelInstance* instance);

    // Slot bookkeeping for contexts that carry global view model instances.
    // A "slot key" is the declared view model id a slot represents; it lets us
    // place an instance in the right slot (and find/replace it) even when the
    // instance's own view model differs from the slot's (the "override" case).
    // Kept index-aligned with m_ViewModelInstances and allocated lazily, so the
    // many global-free contexts pay only a single null pointer. Resolution
    // never consults it.
    struct GlobalSlots
    {
        std::vector<uint32_t> slotKeys;
    };
    std::unique_ptr<GlobalSlots> m_globalSlots;

    // Ensures m_globalSlots exists and its slotKeys are index-aligned with
    // m_ViewModelInstances, backfilling any missing entries with kNoSlot (they
    // were mains before the first global was slotted).
    void ensureGlobalSlots();
    // The slot key for the instance at index i (its stored key when tracked,
    // otherwise kNoSlot, meaning the entry is a main / not on the slot keys).
    uint32_t slotKeyAt(size_t index) const;
    // Insert/remove an instance while keeping slot keys aligned.
    void insertInstanceAt(size_t index,
                          rcp<ViewModelInstance> value,
                          uint32_t slotKey);
    void removeInstanceAt(size_t index);

    ViewModelInstanceValue* tryGetViewModelProperty(
        rcp<ViewModelInstance> instance,
        const std::vector<uint32_t>& path) const;
    ViewModelInstanceValue* tryGetRelativeViewModelProperty(
        rcp<ViewModelInstance> instance,
        const std::vector<uint32_t>& path,
        DataResolver* resolver) const;
    rcp<ViewModelInstance> tryGetViewModelInstance(
        rcp<ViewModelInstance> instance,
        const std::vector<uint32_t>& path) const;
    rcp<ViewModelInstance> tryGetRelativeViewModelInstance(
        rcp<ViewModelInstance> instance,
        const std::vector<uint32_t>& path,
        DataResolver* resolver) const;

public:
    DataContext(rcp<ViewModelInstance> viewModelInstance);
    DataContext(std::vector<rcp<ViewModelInstance>> viewModelInstances);
    ~DataContext();

    // Registers a container as a dependent of this context. Idempotent; also
    // registers it against every instance currently held.
    void addDependentContainer(DataBindContainer* container);
    // Unregisters a container from this context and from every instance it
    // holds. Call before the container is destroyed.
    void removeDependentContainer(DataBindContainer* container);

    rcp<DataContext> parent() { return m_Parent; }
    void parent(rcp<DataContext> value) { m_Parent = value; }
    ViewModelInstanceValue* getViewModelProperty(
        const std::vector<uint32_t> path) const;
    ViewModelInstanceValue* getRelativeViewModelProperty(
        const std::vector<uint32_t> path,
        DataResolver* resolver) const;
    ViewModelInstanceValue* getViewModelProperty(DataBindPath* dataBindPath);
    rcp<ViewModelInstance> getViewModelInstance(
        const std::vector<uint32_t> path) const;
    rcp<ViewModelInstance> getViewModelInstance(DataBindPath*) const;
    rcp<ViewModelInstance> getRelativeViewModelInstance(
        const std::vector<uint32_t> path,
        DataResolver* resolver) const;
    void viewModelInstance(rcp<ViewModelInstance> value);
    // Insert-or-replace the instance occupying `slotKey`. Positions by
    // canonical order (non-global/main first, then ascending slotKey). The
    // instance's own viewModelId is irrelevant to placement, so any instance
    // can fill any slot.
    void setViewModelInstanceForSlot(uint32_t slotKey,
                                     rcp<ViewModelInstance> value);
    // The instance currently occupying `slotKey`, or nullptr.
    rcp<ViewModelInstance> instanceForSlot(uint32_t slotKey) const;
    // Removes the main view model instance(s): every entry not on the slot
    // keys.
    void removeMainViewModelInstance();
    // Replaces the single "main" view model instance (the entry not on the slot
    // keys), preserving any global instances and their slot order. The result
    // is always ordered as [main, globals...].
    void setMainViewModelInstance(rcp<ViewModelInstance> value);
    void advanced();
    // The main view model instance: the first entry not on the slot keys, or
    // nullptr if the context holds only globals.
    rcp<ViewModelInstance> mainViewModelInstance() const;
    const std::vector<rcp<ViewModelInstance>>& viewModelInstances() const
    {
        return m_ViewModelInstances;
    }
    rcp<ViewModelInstance> rootViewModelInstance()
    {
        if (m_Parent)
        {
            return m_Parent->rootViewModelInstance();
        }
        return mainViewModelInstance();
    };

    ViewModelInstanceValue* viewModelValue()
    {
        if (m_Parent)
        {
            return m_Parent->viewModelValue();
        }
        return nullptr;
    }
};
} // namespace rive
#endif
