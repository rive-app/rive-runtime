#include "rive/data_bind/data_bind_container.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_context.hpp"

using namespace rive;

void DataBindContainer::deleteDataBinds()
{
    for (auto& dataBind : m_dataBinds)
    {
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
    // m_persistingDataBinds / m_dirtyDataBinds here would invalidate the
    // active iterators.
    if (m_isProcessing)
    {
        m_pendingRemovals.push_back(dataBind);
        return;
    }
    auto eraseOne = [dataBind](std::vector<DataBind*>& v) {
        v.erase(std::remove(v.begin(), v.end(), dataBind), v.end());
    };
    eraseOne(m_dataBinds);
    if (dataBind->inPersistingList())
    {
        eraseOne(m_persistingDataBinds);
        dataBind->inPersistingList(false);
    }
    if (dataBind->inDirtyList())
    {
        // Membership flag doesn't distinguish dirty vs pending-dirty, so check
        // both — but only when the flag says one of them contains it.
        eraseOne(m_dirtyDataBinds);
        eraseOne(m_pendingDirtyDataBinds);
        dataBind->inDirtyList(false);
    }
    dataBind->container(nullptr);
}

void DataBindContainer::addDataBind(DataBind* dataBind)
{
    // Defer if we're mid-iteration in updateDataBinds; push_back on
    // m_persistingDataBinds during iteration could reallocate and invalidate
    // the active range-for iterator, and the synchronous updateDataBind() call
    // below would re-enter the update machinery.
    if (m_isProcessing)
    {
        m_pendingAdditions.push_back(dataBind);
        return;
    }
    m_dataBinds.push_back(dataBind);
    // Any data bind that is applied to source needs to be updated regardless of
    // it having dirt or not. The reason is that they depend on changes of the
    // value of the target, which is not propagated as dirt to the data binding
    // object. That's why there is a separate list for these that is constantly
    // updated.
    if (dataBind->toSource())
    {
        m_persistingDataBinds.push_back(dataBind);
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

    if (applyTargetToSource && !dataBind->sourceToTargetRunsFirst())
    {

        dataBind->updateSourceBinding();
    }
    if (d != ComponentDirt::None)
    {
        dataBind->dirt(ComponentDirt::None);
        dataBind->update(d);
    }
    if (applyTargetToSource && dataBind->sourceToTargetRunsFirst())
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
    if (m_persistingDataBinds.size() == 0 && m_dirtyDataBinds.size() == 0)
    {
        return;
    }
    m_isProcessing = true;
    for (auto& dataBind : m_persistingDataBinds)
    {
        if (!dataBind->canSkip())
        {
            updateDataBind(dataBind, applyTargetToSource);
        }
    }
    for (auto& dataBind : m_dirtyDataBinds)
    {
        // data binds on this list don't need to apply target to source because
        // any data bind that applies to source is collected on the
        // m_persistingDataBinds list
        dataBind->inDirtyList(false);
        updateDataBind(dataBind, false);
    }
    m_dirtyDataBinds.clear();
    if (m_pendingDirtyDataBinds.size() > 0)
    {
        m_dirtyDataBinds.swap(m_pendingDirtyDataBinds);
    }
    m_isProcessing = false;
    // Flush additions before removals so a same-tick add-then-remove of the
    // same bind resolves in chronological order (add wins, then remove).
    if (!m_pendingAdditions.empty())
    {
        std::vector<DataBind*> additions;
        additions.swap(m_pendingAdditions);
        for (auto* dataBind : additions)
        {
            addDataBind(dataBind);
        }
    }
    if (!m_pendingRemovals.empty())
    {
        std::vector<DataBind*> removals;
        removals.swap(m_pendingRemovals);
        for (auto* dataBind : removals)
        {
            removeDataBind(dataBind);
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
    if (dataBind->toSource())
    {
        return;
    }
    if (dataBind->inDirtyList())
    {
        return;
    }
    auto& insertingList =
        m_isProcessing ? m_pendingDirtyDataBinds : m_dirtyDataBinds;
    insertingList.push_back(dataBind);
    dataBind->inDirtyList(true);
}