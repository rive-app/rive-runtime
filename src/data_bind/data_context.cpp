#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_bind_path.hpp"
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include <algorithm>

using namespace rive;

DataContext::DataContext(rcp<ViewModelInstance> viewModelInstance)
{
    if (viewModelInstance != nullptr)
    {
        m_ViewModelInstances.push_back(std::move(viewModelInstance));
    }
}

DataContext::DataContext(
    std::vector<rcp<ViewModelInstance>> viewModelInstances) :
    m_ViewModelInstances(std::move(viewModelInstances))
{}

DataContext::~DataContext()
{
    // Defensive: detach any still-registered containers so a surviving instance
    // (e.g. one a user still holds) can't keep a stale pointer to a container
    // that is going away with this context.
    for (auto& instance : m_ViewModelInstances)
    {
        detachContainers(instance.get());
    }
}

void DataContext::attachContainers(ViewModelInstance* instance)
{
    if (instance == nullptr)
    {
        return;
    }
    for (auto* container : m_dependentContainers)
    {
        instance->addDependent(container);
    }
}

void DataContext::detachContainers(ViewModelInstance* instance)
{
    if (instance == nullptr)
    {
        return;
    }
    for (auto* container : m_dependentContainers)
    {
        instance->removeDependent(container);
    }
}

void DataContext::addDependentContainer(DataBindContainer* container)
{
    if (container == nullptr)
    {
        return;
    }
    if (std::find(m_dependentContainers.begin(),
                  m_dependentContainers.end(),
                  container) != m_dependentContainers.end())
    {
        return;
    }
    m_dependentContainers.push_back(container);
    for (auto& instance : m_ViewModelInstances)
    {
        if (instance != nullptr)
        {
            instance->addDependent(container);
        }
    }
}

void DataContext::removeDependentContainer(DataBindContainer* container)
{
    for (auto& instance : m_ViewModelInstances)
    {
        if (instance != nullptr)
        {
            instance->removeDependent(container);
        }
    }
    m_dependentContainers.erase(std::remove(m_dependentContainers.begin(),
                                            m_dependentContainers.end(),
                                            container),
                                m_dependentContainers.end());
}

void DataContext::ensureGlobalSlots()
{
    if (m_globalSlots != nullptr)
    {
        // Already tracking; kept index-aligned by the insert/remove primitives.
        return;
    }
    m_globalSlots = std::make_unique<GlobalSlots>();
    // Everything already in the context was a main before the first global was
    // slotted, so backfill each entry as unslotted.
    m_globalSlots->slotKeys.assign(m_ViewModelInstances.size(), kNoSlot);
}

uint32_t DataContext::slotKeyAt(size_t index) const
{
    if (m_globalSlots != nullptr && index < m_globalSlots->slotKeys.size())
    {
        return m_globalSlots->slotKeys[index];
    }
    // Untracked entries are mains (not on the slot keys).
    return kNoSlot;
}

void DataContext::insertInstanceAt(size_t index,
                                   rcp<ViewModelInstance> value,
                                   uint32_t slotKey)
{
    auto it = m_ViewModelInstances.insert(m_ViewModelInstances.begin() + index,
                                          std::move(value));
    if (m_globalSlots != nullptr)
    {
        m_globalSlots->slotKeys.insert(m_globalSlots->slotKeys.begin() + index,
                                       slotKey);
    }
    // Keep all attached containers registered as dependents of the new entry.
    attachContainers(it->get());
}

void DataContext::removeInstanceAt(size_t index)
{
    // Detach before erasing so no container stays registered on the removed
    // instance (which may outlive the context via a user-held reference).
    detachContainers(m_ViewModelInstances[index].get());
    m_ViewModelInstances.erase(m_ViewModelInstances.begin() + index);
    if (m_globalSlots != nullptr)
    {
        m_globalSlots->slotKeys.erase(m_globalSlots->slotKeys.begin() + index);
    }
}

void DataContext::viewModelInstance(rcp<ViewModelInstance> value)
{
    if (m_globalSlots != nullptr)
    {
        setMainViewModelInstance(std::move(value));
        return;
    }
    if (m_ViewModelInstances.empty())
    {
        m_ViewModelInstances.push_back(std::move(value));
        attachContainers(m_ViewModelInstances.back().get());
    }
    else
    {
        detachContainers(m_ViewModelInstances[0].get());
        m_ViewModelInstances[0] = std::move(value);
        attachContainers(m_ViewModelInstances[0].get());
    }
}

void DataContext::setViewModelInstanceForSlot(uint32_t slotKey,
                                              rcp<ViewModelInstance> value)
{
    if (value == nullptr)
    {
        // A null instance empties the slot: drop any occupant so the slot reads
        // as unset. With no global slots tracked there is nothing to clear.
        if (m_globalSlots == nullptr)
        {
            return;
        }
        for (size_t i = 0; i < m_ViewModelInstances.size(); i++)
        {
            if (slotKeyAt(i) == slotKey)
            {
                removeInstanceAt(i);
                return;
            }
        }
        return;
    }
    // Holding a slot-addressed instance makes this a global-bearing context.
    ensureGlobalSlots();
    // Replace in place if the slot is already occupied (keeps its position).
    for (size_t i = 0; i < m_ViewModelInstances.size(); i++)
    {
        if (slotKeyAt(i) == slotKey)
        {
            detachContainers(m_ViewModelInstances[i].get());
            m_ViewModelInstances[i] = std::move(value);
            m_globalSlots->slotKeys[i] = slotKey;
            attachContainers(m_ViewModelInstances[i].get());
            return;
        }
    }
    // Insert at canonical position: keep any leading main (unslotted) instance
    // ahead of the globals, then order among globals by ascending slot key. The
    // instance's own viewModelId is irrelevant here — placement follows
    // slotKey.
    size_t index = 0;
    while (index < m_ViewModelInstances.size() && slotKeyAt(index) == kNoSlot)
    {
        index++;
    }
    while (index < m_ViewModelInstances.size() && slotKeyAt(index) != kNoSlot &&
           slotKeyAt(index) < slotKey)
    {
        index++;
    }
    insertInstanceAt(index, std::move(value), slotKey);
}

rcp<ViewModelInstance> DataContext::instanceForSlot(uint32_t slotKey) const
{
    for (size_t i = 0; i < m_ViewModelInstances.size(); i++)
    {
        if (slotKeyAt(i) == slotKey)
        {
            return m_ViewModelInstances[i];
        }
    }
    return nullptr;
}

void DataContext::removeMainViewModelInstance()
{
    for (size_t i = 0; i < m_ViewModelInstances.size();)
    {
        if (slotKeyAt(i) == kNoSlot)
        {
            removeInstanceAt(i);
        }
        else
        {
            i++;
        }
    }
}

void DataContext::setMainViewModelInstance(rcp<ViewModelInstance> value)
{
    // Drop any existing main (unslotted) entries, then place the new main at
    // the front so resolution order is [main, globals...].
    removeMainViewModelInstance();
    if (value != nullptr)
    {
        // Main is not on the slot keys.
        insertInstanceAt(0, std::move(value), kNoSlot);
    }
}

rcp<ViewModelInstance> DataContext::mainViewModelInstance() const
{
    for (size_t i = 0; i < m_ViewModelInstances.size(); i++)
    {
        if (slotKeyAt(i) == kNoSlot)
        {
            return m_ViewModelInstances[i];
        }
    }
    return nullptr;
}

void DataContext::advanced()
{
    for (auto& vmi : m_ViewModelInstances)
    {
        vmi->advanced();
    }
}

// --- Private helpers: try resolution against a single instance ---

ViewModelInstanceValue* DataContext::tryGetViewModelProperty(
    rcp<ViewModelInstance> instance,
    const std::vector<uint32_t>& path) const
{
    if (instance == nullptr || instance->viewModelId() != path[0])
    {
        return nullptr;
    }
    if (path.size() == 1)
    {
        return nullptr;
    }
    rcp<ViewModelInstance> current = instance;
    for (auto it = path.begin() + 1; it != path.end() - 1; it++)
    {
        auto viewModelInstanceValue = current->propertyValue(*it);
        if (viewModelInstanceValue != nullptr &&
            viewModelInstanceValue->is<ViewModelInstanceViewModel>())
        {
            current = viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                          ->referenceViewModelInstance();
            if (current == nullptr)
            {
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }
    return current->propertyValue(path.back());
}

ViewModelInstanceValue* DataContext::tryGetRelativeViewModelProperty(
    rcp<ViewModelInstance> instance,
    const std::vector<uint32_t>& path,
    DataResolver* resolver) const
{
    if (instance == nullptr)
    {
        return nullptr;
    }
    rcp<ViewModelInstance> current = instance;
    if (path.size() == 1)
    {
        return current->propertyValue(resolver->resolveName(path[0]));
    }
    for (auto it = path.begin(); it != path.end() - 1; it++)
    {
        auto viewModelInstanceValue =
            current->propertyValue(resolver->resolveName(*it));
        if (viewModelInstanceValue != nullptr &&
            viewModelInstanceValue->is<ViewModelInstanceViewModel>())
        {
            current = viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                          ->referenceViewModelInstance();
            if (current == nullptr)
            {
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }
    return current->propertyValue(resolver->resolveName(path.back()));
}

rcp<ViewModelInstance> DataContext::tryGetViewModelInstance(
    rcp<ViewModelInstance> instance,
    const std::vector<uint32_t>& path) const
{
    if (instance == nullptr || instance->viewModelId() != path[0])
    {
        return nullptr;
    }
    rcp<ViewModelInstance> current = instance;
    for (auto it = path.begin() + 1; it != path.end(); it++)
    {
        auto viewModelInstanceValue = current->propertyValue(*it);
        if (viewModelInstanceValue != nullptr &&
            viewModelInstanceValue->is<ViewModelInstanceViewModel>())
        {
            current = viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                          ->referenceViewModelInstance();
            if (current == nullptr)
            {
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }
    return current;
}

rcp<ViewModelInstance> DataContext::tryGetRelativeViewModelInstance(
    rcp<ViewModelInstance> instance,
    const std::vector<uint32_t>& path,
    DataResolver* resolver) const
{
    if (instance == nullptr)
    {
        return nullptr;
    }
    rcp<ViewModelInstance> current = instance;
    for (auto it = path.begin(); it != path.end(); it++)
    {
        auto viewModelInstanceValue =
            current->propertyValue(resolver->resolveName(*it));
        if (viewModelInstanceValue != nullptr &&
            viewModelInstanceValue->is<ViewModelInstanceViewModel>())
        {
            current = viewModelInstanceValue->as<ViewModelInstanceViewModel>()
                          ->referenceViewModelInstance();
            if (current == nullptr)
            {
                return nullptr;
            }
        }
        else
        {
            return nullptr;
        }
    }
    return current;
}

// --- Public resolution methods: iterate instances, then fall to parent ---

ViewModelInstanceValue* DataContext::getViewModelProperty(
    const std::vector<uint32_t> path) const
{
    if (path.size() == 0)
    {
        return nullptr;
    }
    for (auto& instance : m_ViewModelInstances)
    {
        auto result = tryGetViewModelProperty(instance, path);
        if (result != nullptr)
        {
            return result;
        }
    }
    if (m_Parent != nullptr)
    {
        return m_Parent->getViewModelProperty(path);
    }
    return nullptr;
}

ViewModelInstanceValue* DataContext::getRelativeViewModelProperty(
    const std::vector<uint32_t> path,
    DataResolver* resolver) const
{
    if (path.size() == 0 || !resolver)
    {
        return nullptr;
    }
    for (auto& instance : m_ViewModelInstances)
    {
        auto result = tryGetRelativeViewModelProperty(instance, path, resolver);
        if (result != nullptr)
        {
            return result;
        }
    }
    if (m_Parent != nullptr)
    {
        return m_Parent->getRelativeViewModelProperty(path, resolver);
    }
    return nullptr;
}

ViewModelInstanceValue* DataContext::getViewModelProperty(
    DataBindPath* dataBindPath)
{
    if (dataBindPath->isRelative())
    {
        auto file = dataBindPath->file();
        if (file)
        {
            auto resolver = file->dataResolver();
            if (resolver)
            {
                return getRelativeViewModelProperty(
                    dataBindPath->resolvedPath(),
                    resolver);
            }
        }
    }
    return getViewModelProperty(dataBindPath->path());
}

rcp<ViewModelInstance> DataContext::getViewModelInstance(
    const std::vector<uint32_t> path) const
{
    if (path.size() == 0)
    {
        return nullptr;
    }
    for (auto& instance : m_ViewModelInstances)
    {
        auto result = tryGetViewModelInstance(instance, path);
        if (result != nullptr)
        {
            return result;
        }
    }
    if (m_Parent != nullptr)
    {
        return m_Parent->getViewModelInstance(path);
    }
    return nullptr;
}

rcp<ViewModelInstance> DataContext::getRelativeViewModelInstance(
    const std::vector<uint32_t> path,
    DataResolver* resolver) const
{
    if (path.size() == 0 || !resolver)
    {
        return nullptr;
    }
    for (auto& instance : m_ViewModelInstances)
    {
        auto result = tryGetRelativeViewModelInstance(instance, path, resolver);
        if (result != nullptr)
        {
            return result;
        }
    }
    if (m_Parent != nullptr)
    {
        return m_Parent->getRelativeViewModelInstance(path, resolver);
    }
    return nullptr;
}

rcp<ViewModelInstance> DataContext::getViewModelInstance(
    DataBindPath* dataBindPath) const
{
    if (!dataBindPath)
    {
        return nullptr;
    }
    if (dataBindPath->isRelative())
    {
        auto file = dataBindPath->file();
        if (file)
        {
            auto resolver = file->dataResolver();
            if (resolver)
            {
                return getRelativeViewModelInstance(
                    dataBindPath->resolvedPath(),
                    resolver);
            }
        }
        return nullptr;
    }
    else
    {
        return getViewModelInstance(dataBindPath->resolvedPath());
    }
}
