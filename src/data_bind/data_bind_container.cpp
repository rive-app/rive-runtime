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
}

bool DataBindContainer::advanceDataBinds(float elapsedSeconds)
{
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

void DataBindContainer::addDataBind(DataBind* dataBind)
{
    m_dataBinds.push_back(dataBind);
    // Any data bind that is applied to source needs to be updated regardless of
    // it having dirt or not. The reason is that they depend on changes of the
    // value of the target, which is not propagated as dirt to the data binding
    // object. That's why there is a separate list for these that is constantly
    // updated.
    if (dataBind->toSource())
    {
        m_persistingDataBinds.push_back(dataBind);
    }
    else
    {
        m_dirtyDataBinds.push_back(dataBind);
    }
    dataBind->container(this);
}

void DataBindContainer::updateDataBind(DataBind* dataBind,
                                       bool applyTargetToSource)
{
    if (dataBind->canSkip())
    {
        return;
    }
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
    for (auto& dataBind : m_persistingDataBinds)
    {
        updateDataBind(dataBind, applyTargetToSource);
    }
    for (auto& dataBind : m_dirtyDataBinds)
    {
        // data binds on this list don't need to apply target to source because
        // any data bind that applies to source is collected on the
        // m_persistingDataBinds list
        updateDataBind(dataBind, false);
    }
    m_dirtyDataBinds.clear();
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
    auto itr =
        std::find(m_dirtyDataBinds.begin(), m_dirtyDataBinds.end(), dataBind);
    if (itr == m_dirtyDataBinds.end())
    {
        m_dirtyDataBinds.push_back(dataBind);
    }
}