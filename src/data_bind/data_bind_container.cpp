#include "rive/data_bind/data_bind_container.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_context.hpp"

using namespace rive;

// DataBindContainer is a base of Artboard, StateMachineInstance, and every
// DataConverter, so its inline footprint is paid thousands of times over by a
// data-bound artboard component list. Exactly one work queue is inline (see
// DataBindQueues); adding a second costs real memory because it pushes both
// StateMachineInstance and ArtboardInstance into the next allocator size class:
// 368 -> 416 (384 -> 448 allocated) and 1264 -> 1312 (1280 -> 1536).

void DataBindContainer::deleteDataBinds()
{
    for (auto& dataBind : m_dataBinds)
    {
#ifdef WITH_RIVE_EDITOR
        // Skip arena-owned entries — `EditorFile::m_arena` will
        // free them at file destruction. The runtime importer-added
        // entries (no flag set) get deleted as before.
        if (dataBind->isEditorOwned())
        {
            continue;
        }
#endif
        delete dataBind;
    }
}

void DataBindContainer::unbindDataBinds()
{
    for (auto& dataBind : m_dataBinds)
    {
        dataBind->unbind();
    }
    m_dataContext = nullptr;
}

void DataBindContainer::bindDataBindsFromContext(DataContext* dataContext)
{
    for (auto& dataBind : m_dataBinds)
    {
        if (dataBind->is<DataBindContext>())
        {
            dataBind->as<DataBindContext>()->bindFromContext(dataContext);
        }
    }
    m_dataContext = dataContext;
}

bool DataBindContainer::advanceDataBinds(float elapsedSeconds)
{
    if (m_dataBinds.size() == 0)
    {
        return false;
    }
    bool didUpdate = false;
    for (auto& dataBind : m_dataBinds)
    {
        if (dataBind->advance(elapsedSeconds))
        {
            didUpdate = true;
        }
    }
    return didUpdate;
}

void DataBindContainer::removeDataBind(DataBind* dataBind)
{
    // Defer removal if we're mid-iteration in updateDataBinds; erasing from
    // the persisting / dirty queues here would invalidate the active
    // iterators.
    if (m_isProcessing)
    {
        m_queues.ensureAllocated()->pendingRemovals.push_back(dataBind);
        return;
    }
    auto eraseOne = [dataBind](std::vector<DataBind*>& v) {
        v.erase(std::remove(v.begin(), v.end(), dataBind), v.end());
    };
    eraseOne(m_dataBinds);
    // A bind can only be flagged into one of the queues if the queues were
    // allocated to hold it, but the flags live on the bind, so stay defensive.
    auto* queues = m_queues.get();
    if (dataBind->inPersistingList())
    {
        if (queues != nullptr)
        {
            eraseOne(queues->persisting);
        }
        dataBind->inPersistingList(false);
    }
    if (dataBind->inDirtyList())
    {
        // Membership flag doesn't distinguish which dirty list contains the
        // bind, so scan all four — toSource + toTarget × active + pending.
        eraseOne(m_dirtyDataBinds);
        if (queues != nullptr)
        {
            eraseOne(queues->dirtyToSource);
            eraseOne(queues->pendingDirtyToSource);
            eraseOne(queues->pendingDirty);
        }
        dataBind->inDirtyList(false);
    }
    dataBind->container(nullptr);
}

void DataBindContainer::addDataBind(DataBind* dataBind)
{
    // Defer if we're mid-iteration in updateDataBinds; push_back on the
    // persisting list during iteration could reallocate and invalidate the
    // active range-for iterator, and the synchronous updateDataBind() call
    // below would re-enter the update machinery.
    if (m_isProcessing)
    {
        m_queues.ensureAllocated()->pendingAdditions.push_back(dataBind);
        return;
    }
    m_dataBinds.push_back(dataBind);
    // toSource binds: prefer push notifications when the target supports it
    // (Alternative A — Core::notifyPropertyChanged). Fall back to per-frame
    // polling via the persisting list for the few derived/computed targets
    // that don't go through a generated property setter.
    if (dataBind->toSource() && !dataBind->targetSupportsPush())
    {
        m_queues.ensureAllocated()->persisting.push_back(dataBind);
        dataBind->inPersistingList(true);
    }
    dataBind->container(this);
    if (m_dataContext && dataBind->is<DataBindContext>())
    {
        dataBind->as<DataBindContext>()->bindFromContext(m_dataContext);
        updateDataBind(dataBind, true);
    }
}

void DataBindContainer::updateDataBind(DataBind* dataBind,
                                       bool applyTargetToSource)
{
    auto d = dataBind->dirt();

    // Update dependents before applying both target to source and source to
    // target
    if ((d & ComponentDirt::Dependents) == ComponentDirt::Dependents)
    {
        dataBind->updateDependents();
    }

    // Only push target→source when this change actually came from the target
    // (BindingsTarget), or when the target is polled every frame because it
    // can't push (persisting list). Otherwise a source-originated change on a
    // target-first bind would run target→source first and clobber the source's
    // new value with the stale target value before update() propagates it.
    // update() itself stays gated on Bindings (source-originated), so a
    // target-only change is a no-op there.
    bool wantsTargetToSource =
        applyTargetToSource &&
        (dataBind->inPersistingList() ||
         (d & ComponentDirt::BindingsTarget) == ComponentDirt::BindingsTarget);

    if (wantsTargetToSource && !dataBind->sourceToTargetRunsFirst())
    {

        dataBind->updateSourceBinding();
    }
    if (d != ComponentDirt::None)
    {
        dataBind->dirt(ComponentDirt::None);
        dataBind->update(d);
    }
    if (wantsTargetToSource && dataBind->sourceToTargetRunsFirst())
    {

        dataBind->updateSourceBinding();
    }
}

void DataBindContainer::updateDataBinds(bool applyTargetToSource)
{
    // Reject recursive entry. The defer-add / defer-remove machinery depends
    // on m_isProcessing remaining true for the duration of the outer call.
    // A nested call would flip m_isProcessing to false on its return, causing
    // subsequent add/remove calls in the outer iteration to take the immediate
    // path and invalidate the active iterators.
    if (m_isProcessing)
    {
        return;
    }
    {
        // Cheap early-out. The inline dirty list covers the common
        // source→target case; the cold lists only exist once allocated.
        auto* queues = m_queues.get();
        const bool haveColdWork =
            queues != nullptr &&
            (!queues->persisting.empty() || !queues->dirtyToSource.empty());
        if (m_dirtyDataBinds.empty() && !haveColdWork)
        {
            return;
        }
    }
    m_isProcessing = true;
    if (auto* queues = m_queues.get())
    {
        for (auto& dataBind : queues->persisting)
        {
            if (!dataBind->canSkip())
            {
                updateDataBind(dataBind, applyTargetToSource);
            }
        }
        // Push-driven toSource binds, processed before any toTarget binds so
        // their source values land before dependents apply this frame —
        // matches the order the persisting list used to give us under polling.
        for (auto& dataBind : queues->dirtyToSource)
        {
            dataBind->inDirtyList(false);
            updateDataBind(dataBind, applyTargetToSource);
        }
    }
    for (auto& dataBind : m_dirtyDataBinds)
    {
        // Pure toTarget binds — updateSourceBinding is a guarded no-op here.
        dataBind->inDirtyList(false);
        updateDataBind(dataBind, applyTargetToSource);
    }
    m_dirtyDataBinds.clear();
    // Re-read the sidecar rather than reusing a pointer captured above: the
    // loops run with m_isProcessing set, so a re-entrant add / remove / dirty
    // can have allocated the queues even when they started out null.
    if (auto* queues = m_queues.get())
    {
        queues->dirtyToSource.clear();
        if (queues->pendingDirtyToSource.size() > 0)
        {
            queues->dirtyToSource.swap(queues->pendingDirtyToSource);
        }
        if (queues->pendingDirty.size() > 0)
        {
            m_dirtyDataBinds.swap(queues->pendingDirty);
        }
    }
    m_isProcessing = false;
    // Flush additions before removals so a same-tick add-then-remove of the
    // same bind resolves in chronological order (add wins, then remove).
    if (auto* queues = m_queues.get())
    {
        if (!queues->pendingAdditions.empty())
        {
            std::vector<DataBind*> additions;
            additions.swap(queues->pendingAdditions);
            for (auto* dataBind : additions)
            {
                addDataBind(dataBind);
            }
        }
        if (!queues->pendingRemovals.empty())
        {
            std::vector<DataBind*> removals;
            removals.swap(queues->pendingRemovals);
            for (auto* dataBind : removals)
            {
                removeDataBind(dataBind);
            }
        }
    }
}

void DataBindContainer::sortDataBinds()
{
    size_t currentToSourceIndex = 0;
    for (size_t i = 0; i < m_dataBinds.size(); i++)
    {
        if (m_dataBinds[i]->toSource())
        {
            if (i != currentToSourceIndex)
            {

                std::iter_swap(m_dataBinds.begin() + currentToSourceIndex,
                               m_dataBinds.begin() + i);
            }
            currentToSourceIndex += 1;
        }
    }
}

void DataBindContainer::addDirtyDataBind(DataBind* dataBind)
{
    // toSource binds on the polling fallback path are processed via the
    // persisting list — don't also enroll them in the dirty list.
    if (dataBind->toSource() && dataBind->inPersistingList())
    {
        return;
    }
    if (dataBind->inDirtyList())
    {
        return;
    }
    // The common case — a plain source→target bind, outside of a drain — goes
    // straight to the inline list and never touches the sidecar. This is the
    // whole reason m_dirtyDataBinds is not in DataBindQueues.
    if (!dataBind->toSource() && !m_isProcessing)
    {
        m_dirtyDataBinds.push_back(dataBind);
        dataBind->inDirtyList(true);
        return;
    }
    // Push-driven toSource binds go into a dedicated list that
    // updateDataBinds drains *before* the plain dirty list, preserving the
    // ordering polling gave us (target→source first, then source→target).
    // TwoWay binds (both flags set) sit here too — their updateDataBind
    // call runs both directions, but the source-apply happens first.
    auto* queues = m_queues.ensureAllocated();
    auto& insertingList = dataBind->toSource()
                              ? (m_isProcessing ? queues->pendingDirtyToSource
                                                : queues->dirtyToSource)
                              : queues->pendingDirty;
    insertingList.push_back(dataBind);
    dataBind->inDirtyList(true);
}
