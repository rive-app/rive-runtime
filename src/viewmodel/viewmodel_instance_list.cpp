#include <algorithm>
#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

ViewModelInstanceList::~ViewModelInstanceList() { detachParent(); }

void ViewModelInstanceList::detachParent()
{
  if (m_parentViewModelInstance == nullptr)
  {
    return;
  }
  for (auto& item : m_ListItems)
  {
    if (item->viewModelInstance() != nullptr)
    {
      item->viewModelInstance()->removeParent(m_parentViewModelInstance);
    }
  }
  m_parentViewModelInstance = nullptr;
}

void ViewModelInstanceList::propertyValueChanged()
{
  addDirt(ComponentDirt::Bindings);
#ifdef WITH_RIVE_TOOLS
  if (m_changedCallback != nullptr)
  {
    m_changedCallback(this);
  }
#endif
  onValueChanged();
}

// The item's index symbol, when its view model declares one.
static ViewModelInstanceSymbolListIndex* indexSymbol(
    const rcp<ViewModelInstanceListItem>& item)
{
  auto& instance = item->viewModelInstance();
  if (instance == nullptr)
  {
    return nullptr;
  }
  auto symbol = instance->propertyValue(SymbolType::itemIndex);
  if (symbol == nullptr || !symbol->is<ViewModelInstanceSymbolListIndex>())
  {
    return nullptr;
  }
  return symbol->as<ViewModelInstanceSymbolListIndex>();
}

// Lets an item leaving the list report an index change a pass held back,
// so nothing stays deferred once it is gone. Reporting runs the value's
// listeners, which may change this list: never call it while holding an
// iterator or reference into m_ListItems, only an rcp to the item.
static void releaseIndexDelegation(const rcp<ViewModelInstanceListItem>& item)
{
  if (auto* symbol = indexSymbol(item))
  {
    symbol->flushDeferredDelegation();
  }
}

size_t ViewModelInstanceList::writeListIndices(size_t start)
{
  // Listeners are held back, but a tools build's change callback is not,
  // so the live size is read each step in case it takes items out.
  size_t end = m_ListItems.size();
  for (size_t i = start; i < end && i < m_ListItems.size(); i++)
  {
    if (auto* symbol = indexSymbol(m_ListItems[i]))
    {
      symbol->deferDelegation();
      symbol->propertyValue((uint32_t)i);
    }
  }
  return end;
}

void ViewModelInstanceList::reportListIndices(size_t start, size_t end)
{
  for (size_t i = start; i < end && i < m_ListItems.size(); i++)
  {
    auto item = m_ListItems[i];
    releaseIndexDelegation(item);
  }
}

void ViewModelInstanceList::assignListIndices(size_t start)
{
  size_t end = writeListIndices(start);
  reportListIndices(start, end);
}

void ViewModelInstanceList::writeLastIndexOf(
    const rcp<ViewModelInstance>& instance)
{
  // Nothing to write, and no scan, for an instance with no index symbol: a
  // list that never uses indexes keeps its O(1) pop.
  if (instance == nullptr)
  {
    return;
  }
  auto symbol = instance->propertyValue(SymbolType::itemIndex);
  if (symbol == nullptr || !symbol->is<ViewModelInstanceSymbolListIndex>())
  {
    return;
  }
  for (size_t i = m_ListItems.size(); i-- > 0;)
  {
    if (m_ListItems[i]->viewModelInstance() == instance)
    {
      symbol->deferDelegation();
      symbol->as<ViewModelInstanceSymbolListIndex>()->propertyValue(
          (uint32_t)i);
      return;
    }
  }
}

void ViewModelInstanceList::addItem(rcp<ViewModelInstanceListItem> item)
{
  m_ListItems.push_back(item);
  if (item->viewModelInstance())
  {
    item->viewModelInstance()->addParent(parentViewModelInstance());
  }
  item->assignListIndex((uint32_t)(m_ListItems.size() - 1));
  propertyValueChanged();
}

bool ViewModelInstanceList::addItemAt(
    rcp<ViewModelInstanceListItem> item,
    int index)
{
  if (index >= 0 && index <= m_ListItems.size())
  {
    m_ListItems.insert(m_ListItems.begin() + index, item);

    if (item->viewModelInstance())
    {
      item->viewModelInstance()->addParent(parentViewModelInstance());
    }
    assignListIndices((size_t)index);
    propertyValueChanged();
    return true;
  }
  return false;
}

void ViewModelInstanceList::internalAddItem(rcp<ViewModelInstanceListItem> item)
{
  // Copy the supplied owning reference, just as addItem does.
  m_ListItems.push_back(item);
  if (item->viewModelInstance())
  {
    item->viewModelInstance()->addParent(parentViewModelInstance());
  }
  item->assignListIndex((uint32_t)(m_ListItems.size() - 1));
}

void ViewModelInstanceList::removeItem(int index)
{
  // TODO: @hernan decide if we want to return a boolean
  if (index >= 0 && index < m_ListItems.size())
  {
    auto listItem = m_ListItems[index];
    // Held past the erase: the item may be the instance's last owner.
    rcp<ViewModelInstance> instance = listItem->viewModelInstance();
    if (instance != nullptr)
    {
      instance->removeParent(parentViewModelInstance());
    }
    m_ListItems.erase(m_ListItems.begin() + index);
    // Everything is written, the suffix and the removed instance's
    // surviving position, before anything is reported, so a listener finds
    // the list numbered. The removed item's own report (which is also the
    // surviving occurrence's, when they share an instance) comes last.
    size_t end = writeListIndices((size_t)index);
    writeLastIndexOf(instance);
    reportListIndices((size_t)index, end);
    releaseIndexDelegation(listItem);
    propertyValueChanged();
  }
}

rcp<ViewModelInstanceListItem> ViewModelInstanceList::pop()
{
  if (m_ListItems.size() > 0)
  {

    auto listItem = m_ListItems.back();
    removeItem((int)m_ListItems.size() - 1);
    return listItem;
  }
  return nullptr;
}

rcp<ViewModelInstanceListItem> ViewModelInstanceList::shift()
{
  if (m_ListItems.size() > 0)
  {

    auto listItem = m_ListItems.front();
    removeItem(0);
    return listItem;
  }
  return nullptr;
}

void ViewModelInstanceList::removeItem(rcp<ViewModelInstanceListItem> listItem)
{
  auto first = std::find(m_ListItems.begin(), m_ListItems.end(), listItem);
  size_t firstIndex = (size_t)(first - m_ListItems.begin());
  auto noSpaceEnd = std::remove(first, m_ListItems.end(), listItem);
  m_ListItems.erase(noSpaceEnd, m_ListItems.end());
  if (listItem->viewModelInstance())
  {
    listItem->viewModelInstance()->removeParent(parentViewModelInstance());
  }
  size_t end = writeListIndices(firstIndex);
  writeLastIndexOf(listItem->viewModelInstance());
  reportListIndices(firstIndex, end);
  releaseIndexDelegation(listItem);
  propertyValueChanged();
}

void ViewModelInstanceList::removeAllItems()
{
  if (m_ListItems.empty())
  {
    return;
  }
  // Held-back index changes are reported first, while the list is still
  // whole and numbered. A listener may change this list, so this reads the
  // live size and holds each item rather than an iterator.
  for (size_t i = 0; i < m_ListItems.size(); i++)
  {
    auto item = m_ListItems[i];
    releaseIndexDelegation(item);
  }
  // No listener runs from here to the end.
  for (auto& listItem : m_ListItems)
  {
    if (listItem->viewModelInstance())
    {
      listItem->viewModelInstance()->removeParent(parentViewModelInstance());
    }
  }
  m_ListItems.clear();
  propertyValueChanged();
}

void ViewModelInstanceList::removeAllItemsWithViewModelInstance(
    ViewModelInstance* viewModelInstance,
    std::vector<rcp<ViewModelInstanceListItem>>* removed)
{
  if (viewModelInstance == nullptr || m_ListItems.empty())
  {
    return;
  }
  // Every item taken out shares this instance's one index symbol. A change
  // a pass held back is reported first, while the list is whole, since the
  // listener may change this list and the compaction below holds
  // iterators. The instance is kept alive through that in case the listener
  // takes its last item out.
  rcp<ViewModelInstance> instance = ref_rcp(viewModelInstance);
  if (auto symbol = instance->propertyValue(SymbolType::itemIndex))
  {
    symbol->flushDeferredDelegation();
  }
  if (m_ListItems.empty())
  {
    return;
  }

  // The items kept slide down over the ones taken out, so the pass erases
  // once at the end instead of shifting the tail for every match. No
  // listener runs until the renumbering at the end.
  auto kept = m_ListItems.begin();
  size_t firstRemoved = m_ListItems.size();
  for (auto it = m_ListItems.begin(); it != m_ListItems.end(); ++it)
  {
    if ((*it)->viewModelInstance().get() == viewModelInstance)
    {
      firstRemoved = std::min(firstRemoved, (size_t)(it - m_ListItems.begin()));
      viewModelInstance->removeParent(parentViewModelInstance());
      if (removed != nullptr)
      {
        removed->push_back(std::move(*it));
      }
      continue;
    }
    if (kept != it)
    {
      *kept = std::move(*it);
    }
    ++kept;
  }
  if (kept != m_ListItems.end())
  {
    m_ListItems.erase(kept, m_ListItems.end());
    assignListIndices(firstRemoved);
    propertyValueChanged();
  }
}

rcp<ViewModelInstanceListItem> ViewModelInstanceList::item(uint32_t index)
{
  if (index < m_ListItems.size())
  {
    return m_ListItems[index];
  }
  return nullptr;
}

void ViewModelInstanceList::swap(uint32_t index1, uint32_t index2)
{
  if (index1 < m_ListItems.size() && index2 < m_ListItems.size())
  {
    std::iter_swap(m_ListItems.begin() + index1, m_ListItems.begin() + index2);
    // From the lower position on, as the host's pass does, so an instance
    // shown twice keeps the index of its last occurrence.
    assignListIndices(std::min(index1, index2));
    propertyValueChanged();
  }
}

void ViewModelInstanceList::updateList(
    std::vector<rcp<ViewModelInstanceListItem>>* list)
{
  if (list == nullptr)
  {
    return;
  }

  // Held-back index changes of the items leaving are reported first, while
  // the list is still whole and numbered: a listener may change this list,
  // and the loops below hold iterators.
  for (size_t i = 0; i < m_ListItems.size(); i++)
  {
    auto item = m_ListItems[i];
    releaseIndexDelegation(item);
  }

  // Detach old children from this host instance before replacing the list.
  // No listener runs from here until the renumbering.
  if (parentViewModelInstance())
  {
    for (auto& item : m_ListItems)
    {
      if (item->viewModelInstance() != nullptr)
      {
        item->viewModelInstance()->removeParent(parentViewModelInstance());
      }
    }
  }

  m_ListItems.clear();
  m_ListItems.reserve(list->size());
  for (auto& item : *list)
  {
    m_ListItems.push_back(item);
    if (parentViewModelInstance() && item->viewModelInstance() != nullptr)
    {
      item->viewModelInstance()->addParent(parentViewModelInstance());
    }
  }
  assignListIndices(0);
  propertyValueChanged();
}

Core* ViewModelInstanceList::clone() const
{
  auto cloned = new ViewModelInstanceList();
  cloned->copy(*this);
  for (auto property : m_ListItems)
  {
    auto clonedValue = property->clone()->as<ViewModelInstanceListItem>();
    cloned->internalAddItem(rcp<ViewModelInstanceListItem>(clonedValue));
  }
  return cloned;
}

#ifdef WITH_RIVE_EDITOR
void ViewModelInstanceList::editorPrepareForDestruction()
{
  detachParent();
  m_ListItems.clear();
  ViewModelInstanceValue::editorPrepareForDestruction();
}

#endif
