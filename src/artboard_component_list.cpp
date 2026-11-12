#include "rive/component.hpp"
#include "rive/file.hpp"
#include "rive/text/text_language.hpp"
#include "rive/text_engine.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/list_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/math/bitwise.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_number_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_string_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_color_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_boolean_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_enum_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_list_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_trigger_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/semantic/semantic_manager.hpp"
#include "rive/layout_component.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/viewmodel/viewmodel_value_dependent.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/artboard_list_map_rule.hpp"
#include "rive/semantic/semantic_data.hpp"

using namespace rive;

namespace
{
// A row being built on this thread. Building a row binds it, and the lists
// inside it build their rows during that bind, so the rows still being built
// are exactly the ones on the stack: each links to the one it is built in.
// A row's host is set only once its bind returns, so this chain is the only
// record of where they nest. They live in the frames that build them, so
// tracking them allocates nothing.
struct RowBuild
{
  const Artboard* source;
  const Core* listSource;
  // The list building the row.
  const ArtboardComponentList* list;
  const RowBuild* outer;
};
thread_local const RowBuild* s_rowBuild = nullptr;

class RowBuildScope
{
public:
  RowBuildScope(
      const Artboard* source,
      const Core* listSource,
      const ArtboardComponentList* list)
      : m_build{source, listSource, list, s_rowBuild}
  {
    s_rowBuild = &m_build;
  }
  ~RowBuildScope() { s_rowBuild = m_build.outer; }
  RowBuildScope(const RowBuildScope&) = delete;
  RowBuildScope& operator=(const RowBuildScope&) = delete;

private:
  RowBuild m_build;
};
} // namespace

namespace rive
{
/// Marks the hosting list dirty when a list item's drawIndex VM value changes.
class ArtboardListDrawIndexDependent final : public ViewModelValueDependent
{
public:
  ArtboardListDrawIndexDependent(
      ArtboardComponentList* list,
      ViewModelInstanceValue* value)
      : m_list(list)
      , m_value(ref_rcp(value))
  {
    value->addDependent(this);
  }
  ~ArtboardListDrawIndexDependent() { clear(); }

  void addDirt(ComponentDirt value, bool recurse) override
  {
    if (m_list != nullptr)
    {
      m_list->invalidateOrderedListIndicesCache();
      m_list->addDirt(ComponentDirt::Components, false);
    }
  }
  void relinkDataBind() override {}

  void clear()
  {
    if (m_value != nullptr)
    {
      m_value->removeDependent(this);
      m_value = nullptr;
    }
  }

private:
  ArtboardComponentList* m_list;
  rcp<ViewModelInstanceValue> m_value;
};
} // namespace rive

static std::vector<const ViewModelInstanceListItem*> sortedItems(
    const std::vector<rcp<ViewModelInstanceListItem>>& items)
{
  std::vector<const ViewModelInstanceListItem*> sorted;
  sorted.reserve(items.size());
  for (const auto& item : items)
  {
    sorted.push_back(item.get());
  }
  std::sort(sorted.begin(), sorted.end());
  return sorted;
}

// Shared by every list, so a scroll can't mistake one list's version for
// another's.
static std::atomic<uint32_t> s_itemsVersion{0};

ArtboardComponentList::ArtboardComponentList() {}
ArtboardComponentList::~ArtboardComponentList() { clear(); }

bool ArtboardComponentList::collapse(bool value)
{
  if (!Super::collapse(value))
  {
    return false;
  }

  // The hosted artboard's semantics are hidden with its host; its own
  // components are not collapsed.
  forEachRealized([&](int, ArtboardInstance* nestedArtboard) {
    nestedArtboard->syncSemanticBoundary();
  });
  return true;
}

void ArtboardComponentList::clear()
{
  // Rows being destroyed must not call back into quiet-row bookkeeping.
  resetQuietRows(0);
  // Clean up semantic trees before destroying artboards/state machines.
  for (auto& artboard : m_artboardInstancesMap)
  {
    if (artboard.second != nullptr)
    {
      artboard.second->cleanupSemanticTree();
    }
  }

  clearDrawIndexListeners();
  invalidateOrderedListIndicesCache();
  // Clean up focus trees FIRST to prevent use-after-free when the
  // FocusManager still holds references to FocusNodes.
  for (auto& artboard : m_artboardInstancesMap)
  {
    if (artboard.second != nullptr)
    {
      artboard.second->cleanupFocusTree();
    }
  }
  removeListScopeFocusNode();
  m_listRowFocusNodes.clear();

  // Destroy state machines BEFORE artboards.
  // StateMachineInstance owns FocusListenerGroup objects that hold raw
  // pointers to FocusData (owned by artboards). Destroying artboards first
  // would cause use-after-free when FocusListenerGroup destructor tries to
  // unregister from the already-deleted FocusData.
  for (auto& sm : m_stateMachinesMap)
  {
    sm.second.reset();
  }
  for (auto& artboard : m_artboardInstancesMap)
  {
    artboard.second.reset();
  }
  m_stateMachinesMap.clear();
  m_artboardInstancesByIndex.clear();
  m_realizedIndices.clear();
  m_stateMachinesByIndex.clear();
  m_artboardInstancesMap.clear();
  m_listItems.clear();
  m_listHasDuplicateItems = false;
  m_artboardsMap.clear();
  // Pooled rows destroy their state machines first too (see PooledRow).
  m_rowPool.clear();
  m_artboardOverridesMap.clear();
}

rcp<ViewModelInstanceListItem> ArtboardComponentList::listItem(int index)
{
  if (index < m_listItems.size())
  {
    return m_listItems[index];
  }
  return nullptr;
}
ArtboardInstance* ArtboardComponentList::artboardInstance(int index)
{
  if (index >= 0 && index < m_artboardInstancesByIndex.size())
  {
    return m_artboardInstancesByIndex[index];
  }
  return nullptr;
}
int ArtboardComponentList::indexOfArtboardInstance(
    ArtboardInstance* instance) const
{
  if (instance == nullptr)
  {
    return -1;
  }
  for (size_t i = 0; i < m_listItems.size(); ++i)
  {
    auto itr = m_artboardInstancesMap.find(m_listItems[i]);
    if (itr != m_artboardInstancesMap.end() && itr->second.get() == instance)
    {
      return static_cast<int>(i);
    }
  }
  return -1;
}
StateMachineInstance* ArtboardComponentList::stateMachineInstance(int index)
{
  if (index >= 0 && index < m_stateMachinesByIndex.size())
  {
    return m_stateMachinesByIndex[index];
  }
  return nullptr;
}

void ArtboardComponentList::setRowsForItem(
    int index,
    const rcp<ViewModelInstanceListItem>& item,
    ArtboardInstance* artboard,
    StateMachineInstance* stateMachine)
{
  if (!m_listHasDuplicateItems)
  {
    if (index >= 0 && static_cast<size_t>(index) < m_listItems.size() &&
        m_listItems[index] == item)
    {
      resetQuietRow(index);
      placeInRow(index, artboard);
      m_stateMachinesByIndex[index] = stateMachine;
      markRealized(index, artboard != nullptr);
    }
    return;
  }
  for (size_t i = 0; i < m_listItems.size(); i++)
  {
    if (m_listItems[i] == item)
    {
      resetQuietRow(i);
      placeInRow(i, artboard);
      m_stateMachinesByIndex[i] = stateMachine;
      markRealized(i, artboard != nullptr);
    }
  }
}

#ifdef WITH_RIVE_LAYOUT
void* ArtboardComponentList::layoutNode(int index)
{
  auto artboard = artboardInstance(index);
  if (artboard != nullptr)
  {
    return static_cast<void*>(&artboard->takeLayoutData()->node);
  }
  return nullptr;
}

void ArtboardComponentList::collectLayoutNodes(std::vector<void*>& out)
{
  for (int index : m_realizedIndices)
  {
    if (auto node = layoutNode(index))
    {
      out.push_back(node);
    }
  }
}
#endif

void ArtboardComponentList::markLayoutNodeDirty(
    bool shouldForceUpdateLayoutBounds)
{
  bool parentIsRow = mainAxisIsRow();
  bool parentIsStack = isStack();
  forEachRealized([&](int, ArtboardInstance* artboard) {
    artboard->parentIsRow(parentIsRow);
    artboard->parentIsStack(parentIsStack);
  });
}

enum class WindowState : uint8_t
{
  none,
  realized,
  visible
};

void ArtboardComponentList::clearVirtualWindow()
{
  for (int index : m_windowOrder)
  {
    if (index < (int)m_windowState.size())
    {
      m_windowState[index] = (uint8_t)WindowState::none;
    }
  }
  m_windowOrder.clear();
  m_windowVisibleCount = 0;
  invalidateOrderedListIndicesCache();
}

void ArtboardComponentList::addToVirtualWindow(int index, bool visible)
{
  if (index >= (int)m_windowState.size())
  {
    m_windowState.resize(
        std::max((size_t)index + 1, m_listItems.size()),
        (uint8_t)WindowState::none);
  }
  m_windowState[index] = (uint8_t)(visible ? WindowState::visible
                                           : WindowState::realized);
  m_windowOrder.push_back(index);
  if (visible)
  {
    m_windowVisibleCount++;
  }
  invalidateOrderedListIndicesCache();
}

bool ArtboardComponentList::isWithinVisibleWindow(int index) const
{
  return index >= 0 && index < (int)m_windowState.size() &&
         m_windowState[index] == (uint8_t)WindowState::visible;
}

void ArtboardComponentList::updateLayoutBounds(bool animate)
{
#ifdef WITH_RIVE_LAYOUT
  // Buffered items are realized but off screen. Writing their measured size
  // into m_artboardSizes would feed back into the virtualizer, which sums
  // that same table to pick the visible window - so only visible items
  // report their size.
  const bool hasVirtualWindow = virtualizationEnabled() &&
                                m_windowVisibleCount > 0;
  forEachRealized([&](int i, ArtboardInstance* artboard) {
    artboard->updateLayoutBounds(animate);
    if (!hasVirtualWindow || isWithinVisibleWindow(i))
    {
      auto bounds = artboard->layoutBounds();
      setItemSize(Vec2D(bounds.width(), bounds.height()), i);
    }
  });
#endif
  computeLayoutBounds();
}

#ifdef WITH_RIVE_LAYOUT
bool ArtboardComponentList::cascadeLayoutStyle(
    LayoutStyleInterpolation inheritedInterpolation,
    KeyFrameInterpolator* inheritedInterpolator,
    float inheritedInterpolationTime,
    LayoutDirection direction)
{
  forEachRealized([&](int, ArtboardInstance* artboard) {
    artboard->cascadeLayoutStyle(
        inheritedInterpolation,
        inheritedInterpolator,
        inheritedInterpolationTime,
        direction);
  });
  return false;
}
#endif

bool ArtboardComponentList::syncStyleChanges()
{
  bool changed = false;
  forEachRealized([&](int, ArtboardInstance* artboard) {
    bool artboardChanged = artboard->syncStyleChanges();
    if (artboardChanged)
    {
      changed = true;
    }
  });
  return changed;
}

Artboard* ArtboardComponentList::findArtboard(
    const rcp<ViewModelInstanceListItem>& listItem) const
{
  auto viewModelInstance = listItem->viewModelInstance();
  if (viewModelInstance == nullptr)
  {
    return nullptr;
  }
  auto viewModelId = viewModelInstance->viewModelId();
  // Find artboard in the cached mappings
  auto artboard = m_artboardsMap.find(viewModelId);
  if (artboard != m_artboardsMap.end())
  {
    return artboard->second;
  }
  auto artboards = m_file->artboards();
  // Check if there is a special rule that maps the view model to a specific
  // artboard
  auto listRule = m_artboardMapRules.find(viewModelId);
  if (listRule != m_artboardMapRules.end())
  {
    auto artboardIndex = listRule->second;
    if (artboardIndex < artboards.size())
    {
      auto artboard = artboards[artboardIndex];
      m_artboardsMap[viewModelId] = artboard;
      return artboard;
    }
  }

  // Search for the first artboard that is bound to this view model
  for (auto& artboard : artboards)
  {
    if (artboard->viewModelId() == viewModelId)
    {
      m_artboardsMap[viewModelId] = artboard;
      return artboard;
    }
  }

  return nullptr;
}

void ArtboardComponentList::disposeListItem(
    const rcp<ViewModelInstanceListItem>& listItem)
{
  removeArtboard(listItem);
}

// Whether a row of artboard can start over for another item: whatever it
// hosts can. A list does, as the new item's data rebuilds its rows, and so
// does a nested artboard that can remount. An artboard paint keeps a scene
// of its own going.
static bool rowHostsRestart(const Artboard* artboard)
{
#ifdef TESTING
  if (!ArtboardComponentList::sm_reuseHostingRows)
  {
    return false;
  }
#endif
  size_t restarting = artboard->artboardComponentLists().size();
  for (auto* nestedArtboard : artboard->nestedArtboards())
  {
    if (!nestedArtboard->canRemount())
    {
      return false;
    }
    restarting++;
  }
  return restarting == artboard->artboardHosts().size();
}

// Whether a row of artboard scrolls. A scroll constraint's offset and
// momentum aren't values the reuse resets, so another item's row would open
// where this one was scrolled to.
static bool rowScrolls(const Artboard* artboard)
{
  for (auto* object : artboard->objects())
  {
    if (object != nullptr && object->is<ScrollConstraint>())
    {
      return true;
    }
  }
  return false;
}

void ArtboardComponentList::poolListItem(
    const rcp<ViewModelInstanceListItem>& listItem)
{
  auto artboard = findArtboard(listItem);
  auto itr = m_artboardInstancesMap.find(listItem);
  // A row that runs scripts carries their state, and one that scrolls its
  // scroll position, and reuse resets neither; nor the state of what a row
  // hosts, unless it all starts over (see createOrReuseArtboardAt). Such a
  // row is destroyed rather than handed to another item.
  if (artboard == nullptr || itr == m_artboardInstancesMap.end() ||
      itr->second == nullptr || artboard->usesScripts() ||
      rowScrolls(artboard) ||
      (artboard->hostsArtboards() && !rowHostsRestart(artboard)))
  {
    removeArtboard(listItem);
    return;
  }
  // What removeArtboard does for a row it destroys, minus the destroy: the
  // row leaves the semantic and focus trees and its override, then moves to
  // the pool with its state machine.
  auto smItr = m_stateMachinesMap.find(listItem);
  StateMachineInstance* stateMachine = smItr != m_stateMachinesMap.end()
                                           ? smItr->second.get()
                                           : nullptr;
  if (stateMachine != nullptr)
  {
    // Through the state machine, which also forgets the list's manager:
    // reuse hands the same manager back, and an unchanged manager would not
    // build the new item's tree.
    stateMachine->setExternalSemanticManager(nullptr, nullptr);
  }
  itr->second->cleanupSemanticTree();
  itr->second->cleanupFocusTree();
  clearArtboardOverride(itr->second.get());
  PooledRow pooled;
  pooled.artboard = std::move(itr->second);
  // A row that hosts artboards gets a new state machine when it is reused,
  // built on what it hosts then; removeArtboard destroys this one.
  if (stateMachine != nullptr && !artboard->hostsArtboards())
  {
    pooled.stateMachine = std::move(smItr->second);
  }
  m_rowPool[artboard].push_back(std::move(pooled));
  removeArtboard(listItem);
}

void ArtboardComponentList::createOrReuseArtboardAt(int index)
{
  auto item = listItem(index);
  auto artboard = item != nullptr ? findArtboard(item) : nullptr;
  if (artboard == nullptr)
  {
    createArtboardAt(index, false);
    return;
  }
  auto poolItr = m_rowPool.find(artboard);
  if (poolItr == m_rowPool.end() || poolItr->second.empty())
  {
    createArtboardAt(index, false);
    return;
  }
  createArtboardRecorders(artboard);
  auto& pool = poolItr->second;
  auto pooled = std::move(pool.back());
  pool.pop_back();
  // What the row hosts starts over: its nested artboards are mounted anew,
  // before the recorded values reach their inputs, and its lists rebuild
  // their rows when the row is bound to the new item. The row's state
  // machine, made after, is then built on them as a new row's would be.
  for (auto* nestedArtboard : pooled.artboard->nestedArtboards())
  {
    nestedArtboard->remount();
  }
  // Its lists' rows start over too, the ones for items the new item shares
  // with the old included, as a new row's lists would make them all: they
  // go to their lists' pools, binding the row below hands them out again
  // reset, and the ones it doesn't are released after.
  auto* row = pooled.artboard.get();
  for (auto* list : row->artboardComponentLists())
  {
    list->restartRows();
  }
#ifdef TESTING
  if (artboard->hostsArtboards())
  {
    sm_reusedHostingRows++;
  }
#endif
  applyRecorders(pooled.artboard.get(), artboard);
  attachArtboardOverride(pooled.artboard.get());
  addArtboardAt(
      std::move(pooled.artboard),
      index,
      false,
      std::move(pooled.stateMachine));
  for (auto* list : row->artboardComponentLists())
  {
    list->releasePools();
  }
}

void ArtboardComponentList::restartRows()
{
  if (m_listItems.empty())
  {
    return;
  }
  std::vector<rcp<ViewModelInstanceListItem>> none;
  m_keepPools = true;
  updateList(&none);
  m_keepPools = false;
}

void ArtboardComponentList::releasePools()
{
  // State machines before the artboards they drive (see PooledRow).
  m_rowPool.clear();
}

std::unique_ptr<ArtboardInstance> ArtboardComponentList::createArtboard(
    Component* target,
    rcp<ViewModelInstanceListItem> listItem) const
{
  auto artboard = findArtboard(listItem);
  if (artboard != nullptr)
  {
    auto inst = artboard->instance();
    return inst;
  }
  return nullptr;
}

std::unique_ptr<StateMachineInstance> ArtboardComponentList::
    createStateMachineInstance(Component* target, ArtboardInstance* artboard)
{
  if (artboard != nullptr)
  {
    const int defaultIndex = artboard->defaultStateMachineIndex();
    const size_t stateMachineIndex = defaultIndex >= 0
                                         ? static_cast<size_t>(defaultIndex)
                                         : 0u;
    auto stateMachineInstance = artboard->stateMachineAt(stateMachineIndex);
    linkStateMachineToArtboard(stateMachineInstance.get(), artboard);
    return stateMachineInstance;
  }
  return nullptr;
}

void ArtboardComponentList::ensureListScopeFocusNode(
    FocusManager* focusManager,
    rcp<FocusNode> hostParent)
{
  if (focusManager == nullptr)
  {
    return;
  }
  if (m_listScopeFocusNode == nullptr)
  {
    m_listScopeFocusNode = FocusNode::makeStructuralScope();
    m_listScopeFocusNode->name("ArtboardComponentListScope");
  }
  // Only called from the full build pass (buildFocusTreeVisit), which is
  // the ordering authority: always re-append so the scope sits at the
  // walk's current position
  focusManager->addChild(std::move(hostParent), m_listScopeFocusNode);
  syncListRowNodesWithList(focusManager);
}

void ArtboardComponentList::removeListScopeFocusNode()
{
  for (size_t r = 0; r < m_listRowFocusNodes.size(); r++)
  {
    auto& row = m_listRowFocusNodes[r];
    if (row == nullptr)
    {
      continue;
    }
    if (row->manager() != nullptr)
    {
      row->manager()->removeChild(row);
    }
    row.reset();
  }
  m_listRowFocusNodes.clear();
  if (m_listScopeFocusNode == nullptr)
  {
    return;
  }
  if (m_listScopeFocusNode->manager() != nullptr)
  {
    m_listScopeFocusNode->manager()->removeChild(m_listScopeFocusNode);
  }
  m_listScopeFocusNode.reset();
}

rcp<FocusNode> ArtboardComponentList::makeListRowFocusNode() const
{
  // Structural rows: rows with focusable content are descended through;
  // rows for items with no focusables are skipped entirely instead of
  // becoming invisible focus stops.
  auto node = FocusNode::makeStructuralScope();
  node->name("ArtboardComponentListRow");
  return node;
}

void ArtboardComponentList::reparentListRowsInScope(FocusManager* fm)
{
  if (fm == nullptr || m_listScopeFocusNode == nullptr)
  {
    return;
  }
  for (size_t i = 0; i < m_listRowFocusNodes.size(); i++)
  {
    rcp<FocusNode> row = m_listRowFocusNodes[i];
    if (row == nullptr)
    {
      continue;
    }
    if (row->parent() != nullptr)
    {
      row->removeFromParent();
    }
  }
  for (size_t i = 0; i < m_listRowFocusNodes.size(); i++)
  {
    rcp<FocusNode> row = m_listRowFocusNodes[i];
    if (row == nullptr)
    {
      continue;
    }
    fm->addChild(m_listScopeFocusNode, row, i);
  }
}

namespace
{
// True if the artboard contains any focus content at any depth: authored
// FocusData, or a data-bound nested artboard host (whose persistent scope
// counts as focus content even while its current artboard has no focusables —
// a later swap can materialize focus nodes under it). Recurses through both
// host kinds — single nested artboards and component lists
bool artboardHasFocusContent(Artboard* artboard)
{
  if (artboard == nullptr)
  {
    return false;
  }
  if (artboard->rootFocusDataCount() > 0)
  {
    return true;
  }
  for (auto* host : artboard->nestedArtboards())
  {
    if (host == nullptr)
    {
      continue;
    }
    if (host->isArtboardDataBound())
    {
      return true;
    }
    if (artboardHasFocusContent(host->artboardInstance(0)))
    {
      return true;
    }
  }
  for (auto* list : artboard->artboardComponentLists())
  {
    if (list == nullptr)
    {
      continue;
    }
    // A component list is bound to a VM list property and always owns a
    // structural scope (ensureListScopeFocusNode); its items — and their
    // focusables — can populate at runtime, exactly like a data-bound
    // nested-artboard host swap. Count the list itself as latent focus
    // content (symmetric with the isArtboardDataBound() host check above)
    // so the enclosing item is rebuilt under its row while the inner list
    // is still empty, keeping tab order correct once it fills.
    return true;
  }
  return false;
}
} // namespace

bool ArtboardComponentList::listItemNeedsBuildUnderRow(
    FocusManager* parentFM,
    ArtboardInstance* inst,
    rcp<FocusNode> row) const
{
  if (parentFM == nullptr || inst == nullptr || row == nullptr)
  {
    return false;
  }
  if (inst->focusManager() != parentFM)
  {
    return true;
  }
  // If the artboard has focus content but the row is empty, that content is
  // attached outside the row (setExternalFocusManager builds at the manager
  // root) and must be rebuilt under the row. Focus content includes
  // data-bound host scopes at any depth, not just authored FocusData, so a
  // later swap materializes focus nodes at the row's position.
  if (row->children().empty() && artboardHasFocusContent(inst))
  {
    return true;
  }
  return false;
}

void ArtboardComponentList::syncListRowNodesWithList(FocusManager* fm)
{
  if (m_listItems.empty())
  {
    while (!m_listRowFocusNodes.empty())
    {
      auto row = m_listRowFocusNodes.back();
      m_listRowFocusNodes.pop_back();
      if (row != nullptr && row->manager() != nullptr)
      {
        row->manager()->removeChild(row);
      }
    }
    return;
  }
  if (fm != nullptr && listRowNodesInPlace())
  {
    // The list hasn't changed and its rows already sit under the scope in
    // order, so matching them to the list (O(n^2)) and re-parenting them
    // would rebuild the same tree. This runs whenever a virtualized list
    // realizes rows, so only the realized rows' focus trees get work.
    buildListRowFocusTrees(fm);
    return;
  }
  std::vector<rcp<ViewModelInstanceListItem>> listCopy = m_listItems;
  std::vector<rcp<FocusNode>> rowCopy = m_listRowFocusNodes;
  syncListRowNodesWithList(fm, listCopy, rowCopy);
}

void ArtboardComponentList::syncListRowNodesWithList(
    FocusManager* fm,
    const std::vector<rcp<ViewModelInstanceListItem>>& previousListItems,
    const std::vector<rcp<FocusNode>>& previousRowNodes)
{
  if (fm == nullptr || m_listScopeFocusNode == nullptr)
  {
    return;
  }
  const int n = static_cast<int>(m_listItems.size());
  if (n == 0)
  {
    // The rows leave the scope with their items, as they do below for the
    // items a non-empty list drops. Clearing only our references left them
    // under the scope, so every list emptied and refilled grew it.
    for (const auto& row : previousRowNodes)
    {
      if (row != nullptr && row->manager() != nullptr)
      {
        row->manager()->removeChild(row);
      }
    }
    m_listRowFocusNodes.clear();
    return;
  }

  std::vector<rcp<FocusNode>> newRows(static_cast<size_t>(n), nullptr);
  std::vector<rcp<FocusNode>> pr = previousRowNodes;
  for (int j = 0; j < n; j++)
  {
    for (int k = 0; k < static_cast<int>(previousListItems.size()) &&
                    k < static_cast<int>(pr.size());
         k++)
    {
      if (m_listItems[static_cast<size_t>(j)] ==
          previousListItems[static_cast<size_t>(k)])
      {
        newRows[static_cast<size_t>(j)] = pr[static_cast<size_t>(k)];
        pr[static_cast<size_t>(k)] = nullptr;
        break;
      }
    }
  }
  for (int k = 0; k < static_cast<int>(pr.size()) &&
                  k < static_cast<int>(previousListItems.size());
       k++)
  {
    rcp<FocusNode> unmapped = pr[static_cast<size_t>(k)];
    if (unmapped == nullptr)
    {
      continue;
    }
    bool inNew = false;
    for (int j = 0; j < n; j++)
    {
      if (m_listItems[static_cast<size_t>(j)] ==
          previousListItems[static_cast<size_t>(k)])
      {
        inNew = true;
        break;
      }
    }
    if (!inNew)
    {
      if (unmapped->manager() != nullptr)
      {
        unmapped->manager()->removeChild(unmapped);
      }
    }
  }
  m_listRowFocusNodes = std::move(newRows);
  for (int i = 0; i < n; i++)
  {
    rcp<FocusNode>& row = m_listRowFocusNodes[static_cast<size_t>(i)];
    if (row == nullptr)
    {
      row = makeListRowFocusNode();
    }
  }
  reparentListRowsInScope(fm);
  buildListRowFocusTrees(fm);
}

bool ArtboardComponentList::listRowNodesInPlace() const
{
  if (m_listScopeFocusNode == nullptr ||
      m_listRowFocusNodes.size() != m_listItems.size())
  {
    return false;
  }
  const auto& children = m_listScopeFocusNode->children();
  if (children.size() != m_listRowFocusNodes.size())
  {
    return false;
  }
  for (size_t i = 0; i < children.size(); i++)
  {
    if (m_listRowFocusNodes[i] == nullptr ||
        children[i] != m_listRowFocusNodes[i])
    {
      return false;
    }
  }
  return true;
}

void ArtboardComponentList::buildListRowFocusTrees(FocusManager* fm)
{
  const int n = static_cast<int>(m_listItems.size());
  for (int i = 0; i < n; i++)
  {
    ArtboardInstance* inst = artboardInstance(i);
    if (inst == nullptr)
    {
      continue;
    }
    // A row linked since it was last built has no tree yet (see
    // linkStateMachineToArtboard); this is its one build.
    const bool deferred = inst->focusTreeDeferred();
    rcp<FocusNode> row = m_listRowFocusNodes[static_cast<size_t>(i)];
    if (row == nullptr)
    {
      if (deferred)
      {
        inst->buildFocusTree(fm, nullptr);
      }
      continue;
    }
    // Wire the item's state machine to the shared manager first:
    // setExternalFocusManager rebuilds the item's focus tree at the manager
    // root, so it must run before building under the row. Same "wire first,
    // place last" ordering as buildFocusTreeVisit and updateArtboard.
    auto* smi = stateMachineInstance(i);
    if (smi != nullptr && smi->focusManager() != fm)
    {
      smi->setExternalFocusManager(fm);
    }
    if (listItemNeedsBuildUnderRow(fm, inst, row))
    {
      if (inst->focusManager() != nullptr)
      {
        inst->cleanupFocusTree();
      }
      inst->buildFocusTree(fm, row);
    }
    else if (deferred)
    {
      // Nothing to place under the row: build it where linking used to, at
      // the manager root, which wires the row's nested state machines.
      inst->buildFocusTree(fm, nullptr);
    }
  }
}

void ArtboardComponentList::linkStateMachineToArtboard(
    StateMachineInstance* stateMachineInstance,
    ArtboardInstance* artboardInstance)
{
  if (artboardInstance == nullptr || stateMachineInstance == nullptr)
  {
    return;
  }
  auto dataContext = artboardInstance->dataContext();
  stateMachineInstance->dataContext(dataContext);
  stateMachineInstance->updateDataBinds(false);

  auto* parentArtboard = this->artboard();
  if (parentArtboard != nullptr && parentArtboard->focusManager() != nullptr)
  {
    auto* parentFM = parentArtboard->focusManager();
    if (m_listScopeFocusNode != nullptr &&
        artboardInstance->focusManager() != parentFM)
    {
      // A list with a focus scope syncs its rows at the end of every pass
      // that adds one, and buildListRowFocusTrees then builds each new row's
      // tree under the row's own node. Building it at the manager root here
      // first only had that sync tear it down and build it again, a second
      // walk of every new row. Wire the manager now and leave the one build
      // to the sync.
      artboardInstance->adoptFocusManager(parentFM, false);
    }
    else
    {
      stateMachineInstance->setExternalFocusManager(parentFM);
    }
  }

  // Share parent artboard's semantic manager and build semantic tree
  // reparented under the closest SemanticData in the parent artboard, or
  // its boundary.
  if (parentArtboard != nullptr && parentArtboard->semanticManager() != nullptr)
  {
    auto* parentSM = parentArtboard->semanticManager();
    auto parentNode = SemanticData::findClosestSemanticNode(this);
    stateMachineInstance->setExternalSemanticManager(parentSM, parentNode);
  }
}

bool ArtboardComponentList::listsAreEqual(
    std::vector<rcp<ViewModelInstanceListItem>>* list,
    std::vector<rcp<ViewModelInstanceListItem>>* compared)
{
  if (!list || !compared)
  {
    return false;
  }
  if (list->size() != compared->size())
  {
    return false;
  }
  size_t index = 0;
  for (auto& item : *list)
  {
    if (item != (*compared)[index])
    {
      return false;
    }
    ++index;
  }
  return true;
}

void ArtboardComponentList::updateList(
    std::vector<rcp<ViewModelInstanceListItem>>* list)
{
  if (listsAreEqual(&m_listItems, list))
  {
    return;
  }
  if (!m_notedPointers.empty())
  {
    exitRowsLeaving(*list);
  }
  const std::vector<rcp<ViewModelInstanceListItem>> preListItems = m_listItems;
  const std::vector<rcp<FocusNode>> preRowNodes = m_listRowFocusNodes;
  // Laying out a virtualized list below realizes rows, which would sync the
  // focus rows against the new list with rows still matching the old one;
  // the sync at the end, against the rows the update started from, covers
  // those rows too.
  const bool wasUpdatingList = m_updatingList;
  m_updatingList = true;
  m_listItems.clear();
  m_listItems.assign(list->begin(), list->end());
  const auto sorted = sortedItems(m_listItems);
  // Rows that show the same item share its instances, and keeping them in
  // step takes a scan of every row. Most lists show each item once.
  m_listHasDuplicateItems = std::adjacent_find(sorted.begin(), sorted.end()) !=
                            sorted.end();
  invalidateOrderedListIndicesCache();
  m_artboardSizes.clear();

  // Clear the index vectors - they'll be rebuilt as artboards are created
  resetQuietRows(m_listItems.size());
  m_artboardInstancesByIndex.assign(m_listItems.size(), nullptr);
  m_realizedIndices.clear();
  m_stateMachinesByIndex.assign(m_listItems.size(), nullptr);
  quietUnrealizedRows();

  auto p = layoutParent();
  if (p != nullptr)
  {
#ifdef WITH_RIVE_LAYOUT
    p->clearLayoutChildren();
#endif
  }
  // We need to dispose old items after the layout children of the parent have
  // updated to ensure no bad YGNodes are being hosted from the old data
  // during clearLayoutChildren.
  const bool virtualized = virtualizationEnabled();
  for (const auto& item : preListItems)
  {
    if (!std::binary_search(sorted.begin(), sorted.end(), item.get()))
    {
      if (virtualized)
      {
        disposeListItem(item);
      }
      else
      {
        poolListItem(item);
      }
    }
  }
  uint32_t index = 0;
  for (auto& item : m_listItems)
  {
    item->assignListIndex(index);
    auto artboard = findArtboard(item);
    if (artboard != nullptr)
    {
      m_artboardSizes.push_back(Vec2D(artboard->width(), artboard->height()));
    }
    auto itr = m_artboardInstancesMap.find(item);
    if (itr != m_artboardInstancesMap.end())
    {
      // Existing artboard - update index vectors
      resetQuietRow(index);
      placeInRow(index, itr->second.get());
      markRealized(index, itr->second != nullptr);
      auto smItr = m_stateMachinesMap.find(item);
      if (smItr != m_stateMachinesMap.end())
      {
        m_stateMachinesByIndex[index] = smItr->second.get();
      }
    }
    else if (!virtualized && !refusesNestedRow(artboard))
    {
      // What the row builds while it binds, and while its state machine
      // enters its first state, nests in it.
      RowBuildScope build(artboard, m_listSource, this);
      createOrReuseArtboardAt(index);
      // Enter the new row's initial state now, the way the settle pass would
      // a step later, so the first layout solve and text shape already see
      // what that state keys (font sizes, visibility). Otherwise the row is
      // laid out and shaped twice in its first frame. Only when the host
      // advances the rows' state machines at all: one that doesn't (an
      // editor in design mode) shows rows as authored.
      auto stateMachine = m_stateMachinesByIndex[index];
      if (m_advancesNested && stateMachine != nullptr &&
          stateMachine->tryChangeState())
      {
        stateMachine->advance(0.0f, false);
      }
    }
    index++;
  }
  if (!virtualized && !m_keepPools)
  {
    // Rows nothing took are the ones this update removed for good.
    releasePools();
  }
  m_itemsVersion = ++s_itemsVersion;
  computeLayoutBounds();
  syncLayoutChildren();
  markLayoutNodeDirty();
  markWorldTransformDirty();
  addDirt(ComponentDirt::Components);
  recomputeListUsesDrawIndexSort();
  syncDrawIndexListeners();
  m_updatingList = wasUpdatingList;
  auto* parentAb = artboard();
  if (parentAb != nullptr && parentAb->focusManager() != nullptr &&
      m_listScopeFocusNode != nullptr)
  {
    syncListRowNodesWithList(
        parentAb->focusManager(),
        preListItems,
        preRowNodes);
  }
}

void ArtboardComponentList::syncLayoutChildren()
{
  auto p = layoutParent();
  if (p != nullptr)
  {
#ifdef WITH_RIVE_LAYOUT
    p->syncLayoutChildren();
#endif
  }
}

#ifdef TESTING
uint64_t ArtboardComponentList::sm_quietRowSkips = 0;
bool ArtboardComponentList::sm_quietRowsEnabled = true;
bool ArtboardComponentList::sm_requietAfterMachine = true;
bool ArtboardComponentList::sm_verifyQuietRows = true;
bool ArtboardComponentList::sm_reuseHostingRows = true;
uint64_t ArtboardComponentList::sm_reusedHostingRows = 0;
bool ArtboardComponentList::sm_verifyingQuietRow = false;
#endif

// Whether a row that its machine's advance leaves quiet goes quiet then (see
// advanceComponent). Tests can turn it off.
static bool requietAfterMachine()
{
#ifdef TESTING
  return ArtboardComponentList::sm_requietAfterMachine;
#else
  return true;
#endif
}

bool ArtboardComponentList::refusesNestedRow(const Artboard* source) const
{
  if (source == nullptr)
  {
    return false;
  }
  // A row of the same artboard, made from the same source, around this one:
  // say a row whose own list is bound to the list that made it, found through
  // the parent context. This row would hold the same rows that one does, this
  // one among them, without end. Rows of an artboard nested in its own rows
  // from other sources -- a tree, each row listing its children -- are left to
  // the depth cap: their data can end. So are lists a converter makes, which
  // report no source (see boundListSource).
  auto repeats = [&](const Artboard* aroundSource,
                     const Core* aroundListSource) {
    return m_listSource != nullptr && aroundSource == source &&
           aroundListSource == m_listSource;
  };
  uint32_t depth = 0;
  bool refuses = false;
  // First the rows around this one still being built, innermost first...
  const ArtboardComponentList* outermost = this;
  for (const RowBuild* build = s_rowBuild; build != nullptr && !refuses;
       build = build->outer)
  {
    refuses = repeats(build->source, build->listSource) ||
              ++depth >= maxRowNesting;
    outermost = build->list;
  }
  // ...then the hosted artboards the outermost of them is in, up to the root.
  // A row a virtualized list realizes on a later pass, or that a later data
  // change builds, is built outside any other row's build, and nests in those
  // alone. Only list rows count: the walk goes through nested artboards, but
  // one can't nest itself, so it can't make a loop on its own.
  for (const Artboard* around = outermost->artboard();
       !refuses && around != nullptr && around->host() != nullptr;
       around = around->host()->parentArtboard())
  {
    const ArtboardHost* host = around->host();
    if (host->type() != ArtboardComponentListBase::typeKey)
    {
      continue;
    }
    refuses = repeats(around->artboardSource(), host->hostedRowsSource()) ||
              ++depth >= maxRowNesting;
  }
  if (refuses && m_file != nullptr)
  {
    m_file->noteRefusedNestedListRow();
  }
  return refuses;
}

static size_t lowestSetBit(uint64_t bits)
{
  return static_cast<size_t>(63 - math::clz64(bits & (~bits + 1)));
}

size_t ArtboardComponentList::nextAwakeRow(size_t row) const
{
  const size_t count = m_listItems.size();
  while (row < count)
  {
    const size_t word = row >> 6;
    const uint64_t quiet = word < m_quietRows.size() ? m_quietRows[word] : 0;
    const uint64_t awake = ~quiet & (~uint64_t(0) << (row & 63));
    if (awake != 0)
    {
      return std::min(count, (word << 6) + lowestSetBit(awake));
    }
    row = (word + 1) << 6;
  }
  return count;
}

bool ArtboardComponentList::isRowQuiet(size_t row) const
{
  const size_t word = row >> 6;
  return word < m_quietRows.size() &&
         ((m_quietRows[word] >> (row & 63)) & 1) != 0;
}

AdvancingComponent::QuietState ArtboardComponentList::rowQuietState(size_t row)
{
  using QuietState = AdvancingComponent::QuietState;
  auto artboard = m_artboardInstancesByIndex[row];
  auto stateMachine = m_stateMachinesByIndex[row];
  if (artboard == nullptr)
  {
    // An unrealized row has no work at all until it is realized, which
    // resets it (setRowsForItem).
    return stateMachine == nullptr ? QuietState::quiet : QuietState::busy;
  }
  // Pooled machines pair up with pooled artboards by pool order only.
  if (stateMachine != nullptr && stateMachine->artboard() != artboard)
  {
    return QuietState::busy;
  }
  // The machine first: it is a few flags, and when the row is busy it is
  // usually what is busy. What the artboard hosts is checked through it.
  if (stateMachine != nullptr)
  {
    auto state = stateMachine->rowQuietState();
    if (state != QuietState::quiet)
    {
      return state;
    }
  }
  return artboard->rowQuietState();
}

bool ArtboardComponentList::tryQuietRow(size_t row)
{
  const size_t word = row >> 6;
  const uint64_t bit = uint64_t(1) << (row & 63);
  if (!canQuietRows() || word >= m_quietRows.size() ||
      (m_neverQuietRows[word] & bit) != 0)
  {
    return false;
  }
  switch (rowQuietState(row))
  {
    case AdvancingComponent::QuietState::never:
      m_neverQuietRows[word] |= bit;
      return false;
    case AdvancingComponent::QuietState::busy:
      return false;
    case AdvancingComponent::QuietState::quiet:
      break;
  }
  m_quietRows[word] |= bit;
  if (auto artboard = m_artboardInstancesByIndex[row])
  {
    artboard->quietHostRow(static_cast<uint32_t>(row));
    artboard->markHostedQuiet();
  }
  return true;
}

AdvancingComponent::QuietState ArtboardComponentList::quietState()
{
  using QuietState = AdvancingComponent::QuietState;
  if (artboardCount() == 0)
  {
    return QuietState::quiet;
  }
  // New system languages or fallback fonts: the next advance wakes the rows,
  // whose text shapes with them (see advanceComponent). Collapsed, that
  // waits until the list shows, and whatever shows it wakes the row hosting
  // it.
  if (!isCollapsed() && textEnvironmentChanged())
  {
    return QuietState::busy;
  }
  // Collapsed, the advance only notes where the change sequence stands, for
  // the rows' machines once they run again. That too is nothing while every
  // row is quiet: a change one of them reads wakes it, and so this list's
  // row, whose next advance notes the change. A row woken while the list is
  // collapsed stays awake until it shows.
  return allRowsQuiet() ? QuietState::quiet : QuietState::busy;
}

bool ArtboardComponentList::allRowsQuiet() const
{
  return nextAwakeRow(0) >= m_listItems.size();
}

bool ArtboardComponentList::textEnvironmentChanged() const
{
  if (TextLanguages::generation() != m_languageGeneration)
  {
    return true;
  }
#ifdef WITH_RIVE_TEXT
  if (Font::fallbackFontsGeneration() != m_fallbackFontsGeneration)
  {
    return true;
  }
#endif
  return false;
}

bool ArtboardComponentList::canQuietRows() const
{
#ifdef TESTING
  if (!sm_quietRowsEnabled)
  {
    return false;
  }
#endif
  // A row sharing its item's instances with other rows can't be judged on
  // its own.
  return !m_listHasDuplicateItems;
}

void ArtboardComponentList::quietUnrealizedRows()
{
  if (!canQuietRows() || m_quietRows.empty())
  {
    return;
  }
  std::fill(m_quietRows.begin(), m_quietRows.end(), ~uint64_t(0));
  if (size_t tail = m_listItems.size() & 63)
  {
    m_quietRows.back() = (uint64_t(1) << tail) - 1;
  }
}

void ArtboardComponentList::wakeQuietRows()
{
  for (size_t word = 0; word < m_quietRows.size(); word++)
  {
    for (uint64_t quiet = m_quietRows[word]; quiet != 0; quiet &= quiet - 1)
    {
      const size_t row = (word << 6) + lowestSetBit(quiet);
      if (row < m_artboardInstancesByIndex.size() &&
          m_artboardInstancesByIndex[row] != nullptr)
      {
        m_artboardInstancesByIndex[row]->quietHostRow(Artboard::kNoQuietRow);
      }
    }
    m_quietRows[word] = 0;
  }
}

void ArtboardComponentList::resetQuietRow(size_t row)
{
  const size_t word = row >> 6;
  if (word >= m_quietRows.size())
  {
    return;
  }
  const uint64_t bit = uint64_t(1) << (row & 63);
  if ((m_quietRows[word] & bit) != 0 &&
      row < m_artboardInstancesByIndex.size() &&
      m_artboardInstancesByIndex[row] != nullptr)
  {
    m_artboardInstancesByIndex[row]->quietHostRow(Artboard::kNoQuietRow);
  }
  m_quietRows[word] &= ~bit;
  m_neverQuietRows[word] &= ~bit;
}

void ArtboardComponentList::resetQuietRows(size_t rowCount)
{
  for (size_t word = 0; word < m_quietRows.size(); word++)
  {
    for (uint64_t quiet = m_quietRows[word]; quiet != 0; quiet &= quiet - 1)
    {
      const size_t row = (word << 6) + lowestSetBit(quiet);
      if (row < m_artboardInstancesByIndex.size() &&
          m_artboardInstancesByIndex[row] != nullptr)
      {
        m_artboardInstancesByIndex[row]->quietHostRow(Artboard::kNoQuietRow);
      }
    }
  }
  const size_t words = (rowCount + 63) >> 6;
  m_quietRows.assign(words, 0);
  m_neverQuietRows.assign(words, 0);
}

void ArtboardComponentList::hostedRowWoke(Artboard* artboard, uint32_t row)
{
  // Rows are reset before their instances change, so the artboard is still
  // the row's; checked anyway, since waking a row by mistake only costs a
  // frame of its work.
  if (row < m_artboardInstancesByIndex.size() &&
      m_artboardInstancesByIndex[row] == artboard)
  {
    const size_t word = row >> 6;
    if (word < m_quietRows.size())
    {
      m_quietRows[word] &= ~(uint64_t(1) << (row & 63));
    }
  }
  // The list is no longer quiet, so neither is a row it is on.
  if (auto host = this->artboard())
  {
    host->wakeIfQuietRow();
  }
}

#ifdef TESTING
// Whether every artboard hosted under artboard by nested artboards is marked
// quiet, so a change to one of them wakes the row (see markHostedQuiet).
static bool hostedMarkedQuiet(Artboard* artboard)
{
  for (auto nestedArtboard : artboard->nestedArtboards())
  {
    auto instance = nestedArtboard->artboardInstance();
    if (instance != nullptr &&
        (instance->quietHostRow() != 0 || !hostedMarkedQuiet(instance)))
    {
      return false;
    }
  }
  return true;
}

void ArtboardComponentList::verifyQuietRows(
    RowPass pass,
    float elapsedSeconds,
    AdvanceFlags flags,
    bool advanceNested)
{
  if (!sm_verifyQuietRows)
  {
    return;
  }
  for (size_t word = 0; word < m_quietRows.size(); word++)
  {
    for (uint64_t quiet = m_quietRows[word]; quiet != 0; quiet &= quiet - 1)
    {
      const size_t row = (word << 6) + lowestSetBit(quiet);
      sm_quietRowSkips++;
      auto artboard = m_artboardInstancesByIndex[row];
      auto stateMachine = m_stateMachinesByIndex[row];
      const char* problem = nullptr;
      if (rowQuietState(row) != AdvancingComponent::QuietState::quiet)
      {
        problem = "stopped being quiet but nothing woke it";
      }
      else if (artboard == nullptr)
      {
        // Unrealized, so there is no work to check.
      }
      else if (artboard->quietHostRow() != row)
      {
        problem = "lost track of its artboard";
      }
      else if (!hostedMarkedQuiet(artboard))
      {
        problem = "lost track of an artboard it hosts";
      }
      else
      {
        auto dirt = Artboard::sm_dirtNotifications;
        auto updates = DataBindContainer::sm_dataBindUpdates;
        bool didWork = false;
        // A nested list's verifier runs inside this pass.
        bool wasVerifying = sm_verifyingQuietRow;
        sm_verifyingQuietRow = true;
        switch (pass)
        {
          case RowPass::advance:
            if (advanceNested && stateMachine != nullptr)
            {
              didWork = stateMachine->advance(elapsedSeconds, true);
            }
            didWork = artboard->advanceInternal(elapsedSeconds, flags) ||
                      didWork;
            break;
          case RowPass::settle:
            if (advanceNested && stateMachine != nullptr)
            {
              didWork = stateMachine->tryChangeState();
            }
            didWork = artboard->advanceInternal(elapsedSeconds, flags) ||
                      didWork;
            break;
          case RowPass::updateDataBinds:
            if (stateMachine != nullptr)
            {
              stateMachine->updateDataBinds(false);
            }
            artboard->updateDataBinds();
            break;
          case RowPass::update:
            didWork = artboard->updatePass(false);
            break;
        }
        sm_verifyingQuietRow = wasVerifying;
        if (didWork || dirt != Artboard::sm_dirtNotifications ||
            updates != DataBindContainer::sm_dataBindUpdates ||
            artboard->hasDirt(ComponentDirt::Components) ||
            artboard->quietHostRow() != row ||
            (stateMachine != nullptr &&
             (stateMachine->needsAdvance() ||
              stateMachine->stateChangedCount() != 0)))
        {
          problem = "did work while quiet";
        }
      }
      if (problem != nullptr)
      {
        fprintf(
            stderr,
            "Quiet row %zu of list '%s' %s\n",
            row,
            name().c_str(),
            problem);
        abort();
      }
    }
  }
}
#endif

bool ArtboardComponentList::advanceComponent(
    float elapsedSeconds,
    AdvanceFlags flags)
{
  m_advancesNested = (flags & AdvanceFlags::AdvanceNested) ==
                     AdvanceFlags::AdvanceNested;
  if (artboardCount() == 0)
  {
    return false;
  }
  if (isCollapsed())
  {
#ifdef TESTING
    // Standing in for a skipped pass, which notes nothing.
    if (sm_verifyingQuietRow)
    {
      return false;
    }
#endif
    m_inactiveChangeSequence = ViewModelInstanceValue::latestChangeSequence();
    return false;
  }
  bool keepGoing = false;
  bool advanceNested = (flags & AdvanceFlags::AdvanceNested) ==
                       AdvanceFlags::AdvanceNested;
  bool newFrame = (flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame;
  auto advancingFlags = flags & ~AdvanceFlags::IsRoot;
  // New system languages and fallback fonts reshape text, in quiet rows too.
  if (textEnvironmentChanged())
  {
    m_languageGeneration = TextLanguages::generation();
#ifdef WITH_RIVE_TEXT
    m_fallbackFontsGeneration = Font::fallbackFontsGeneration();
#endif
    wakeQuietRows();
  }
  const size_t count = m_listItems.size();
  for (size_t row = nextAwakeRow(0); row < count; row = nextAwakeRow(row + 1))
  {
    // A row whose work this frame would do nothing sleeps until something
    // wakes it.
    if (newFrame && tryQuietRow(row))
    {
      continue;
    }
    int i = static_cast<int>(row);
    if (advanceNested)
    {
      auto stateMachine = stateMachineInstance(i);
      if (stateMachine != nullptr)
      {
        // If it is not a new frame, we
        // first validate whether their state has changed. Then and only
        // then we advance the state machine. This avoids triggering
        // dirt from advances that make intermediate value changes but
        // finally settle in the same value
        if (!newFrame)
        {
          if (stateMachine->tryChangeState())
          {
            if (stateMachine->advance(elapsedSeconds, newFrame))
            {
              keepGoing = true;
            }
          }
        }
        else
        {
          if (stateMachine->advance(elapsedSeconds, newFrame))
          {
            keepGoing = true;
          }
          // A row woken only for its machine to search again, which found
          // nothing to change, would go quiet at the next frame's start,
          // before its artboard does any more work. It goes quiet now.
          else if (requietAfterMachine() && tryQuietRow(row))
          {
            continue;
          }
        }
      }
    }
    auto artboard = artboardInstance(i);
    if (artboard != nullptr)
    {
      if (artboard->advanceInternal(elapsedSeconds, advancingFlags))
      {
        keepGoing = true;
      }
      if (artboard->hasDirt(ComponentDirt::Components))
      {
        // The animation(s) caused the artboard to need an update.
        addDirt(ComponentDirt::Components);
      }
    }
  }
#ifdef TESTING
  verifyQuietRows(
      newFrame ? RowPass::advance : RowPass::settle,
      elapsedSeconds,
      advancingFlags,
      advanceNested);
#endif
  return keepGoing;
}

void ArtboardComponentList::reset() {}

AABB ArtboardComponentList::layoutBounds()
{
  return AABB(0, 0, m_layoutSize.x, m_layoutSize.y);
}

AABB ArtboardComponentList::layoutBoundsForNode(int index)
{
  if (virtualizationEnabled())
  {
    int count = (int)m_artboardSizes.size();
    if (count == 0)
    {
      return AABB();
    }
    int realIndex = index % count;
    // The scroll's layout places items in virtualized content space,
    // after any scroll children ahead of this list.
    auto position = scrollConstraint()->virtualItemPosition(this, realIndex);
    auto itemSize = m_artboardSizes[realIndex];
    return AABB(
        position.x,
        position.y,
        position.x + itemSize.x,
        position.y + itemSize.y);
  }
  else
  {
    // A row can have no artboard (e.g. its view model resolves to none).
    auto artboard = artboardInstance(index);
    if (artboard != nullptr)
    {
      return artboard->layoutBounds();
    }
  }
  return AABB();
}

#ifdef TESTING
uint32_t ArtboardComponentList::sm_rowScans = 0;
#endif

void ArtboardComponentList::placeInRow(size_t index, ArtboardInstance* artboard)
{
  m_artboardInstancesByIndex[index] = artboard;
  if (artboard != nullptr)
  {
    artboard->listRow((uint32_t)index);
  }
}

bool ArtboardComponentList::showsInARow(ArtboardInstance* artboard)
{
  // Putting an artboard in a row tells it the row. Without repeated items an
  // artboard is in one row at most, the last one it was put in, so a row
  // that no longer holds it means none does.
  uint32_t row = artboard->listRow();
  if (row < m_artboardInstancesByIndex.size() &&
      m_artboardInstancesByIndex[row] == artboard)
  {
    return true;
  }
  if (!m_listHasDuplicateItems)
  {
    return false;
  }
#ifdef TESTING
  sm_rowScans++;
#endif
  return std::any_of(
      m_realizedIndices.begin(),
      m_realizedIndices.end(),
      [&](int i) { return m_artboardInstancesByIndex[i] == artboard; });
}

void ArtboardComponentList::markHostingLayoutDirty(
    ArtboardInstance* artboardInstance)
{
  // Only an artboard showing in a row joins the parent's layout. Artboards
  // in the pools get here too, whenever reusing one resets its properties.
  if (artboardInstance != nullptr && showsInARow(artboardInstance))
  {
    this->artboard()->markLayoutDirty(artboardInstance);
  }
  markWorldTransformDirty();
}

bool ArtboardComponentList::willDraw()
{
  return Super::willDraw() && m_listItems.size() > 0;
}

void ArtboardComponentList::invalidateOrderedListIndicesCache()
{
  m_orderedListIndicesCacheValid = false;
}

void ArtboardComponentList::recomputeListUsesDrawIndexSort()
{
  const bool prevUsesDrawIndexSort = m_listUsesDrawIndexSort;
  m_listUsesDrawIndexSort = false;
  for (const auto& item : m_listItems)
  {
    auto vmi = item->viewModelInstance().get();
    if (vmi == nullptr)
    {
      continue;
    }
    auto* vm = vmi->viewModel();
    if (vm != nullptr && vm->property(SymbolType::drawIndex) != nullptr)
    {
      m_listUsesDrawIndexSort = true;
      if (prevUsesDrawIndexSort != m_listUsesDrawIndexSort)
      {
        invalidateOrderedListIndicesCache();
      }
      return;
    }
  }
  if (prevUsesDrawIndexSort != m_listUsesDrawIndexSort)
  {
    invalidateOrderedListIndicesCache();
  }
}

float ArtboardComponentList::listItemDrawIndex(int index) const
{
  if (index < 0 || static_cast<size_t>(index) >= m_listItems.size())
  {
    return 0.0f;
  }
  auto*
      vmi = m_listItems[static_cast<size_t>(index)]->viewModelInstance().get();
  if (vmi == nullptr)
  {
    return 0.0f;
  }
  auto* vm = vmi->viewModel();
  if (vm == nullptr || vm->property(SymbolType::drawIndex) == nullptr)
  {
    return 0.0f;
  }
  auto* prop = vmi->propertyValue(SymbolType::drawIndex);
  if (prop != nullptr && prop->is<ViewModelInstanceNumber>())
  {
    float v = prop->as<ViewModelInstanceNumber>()->propertyValue();
    if (!std::isfinite(v))
    {
      return 0.0f;
    }
    return v;
  }
  return 0.0f;
}

void ArtboardComponentList::clearDrawIndexListeners()
{
  m_drawIndexDependents.clear();
}

void ArtboardComponentList::removeDrawIndexListenerForItem(
    const rcp<ViewModelInstanceListItem>& listItem)
{
  m_drawIndexDependents.erase(listItem);
}

void ArtboardComponentList::syncDrawIndexListeners()
{
  clearDrawIndexListeners();
  if (!m_listUsesDrawIndexSort)
  {
    return;
  }
  for (const auto& item : m_listItems)
  {
    auto vmi = item->viewModelInstance().get();
    if (vmi == nullptr)
    {
      continue;
    }
    auto* prop = vmi->propertyValue(SymbolType::drawIndex);
    if (prop == nullptr)
    {
      continue;
    }
    m_drawIndexDependents[item] = std::make_unique<
        ArtboardListDrawIndexDependent>(this, prop);
  }
}

void ArtboardComponentList::ensureOrderedListIndices()
{
  const int count = static_cast<int>(m_listItems.size());
  if (count == 0)
  {
    m_orderedListIndicesCacheValid = false;
    m_cachedOrderedListIndices.clear();
    return;
  }

  if (m_orderedListIndicesCacheValid)
  {
    return;
  }

  std::vector<int>& cache = m_cachedOrderedListIndices;
  cache.clear();
  const bool useVirtualWindow = virtualizationEnabled() &&
                                !m_windowOrder.empty();

  if (useVirtualWindow)
  {
    for (int index : m_windowOrder)
    {
      if (index < count)
      {
        cache.push_back(index);
      }
    }
  }
  else
  {
    cache.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++)
    {
      cache.push_back(i);
    }
  }

  if (m_listUsesDrawIndexSort)
  {
    std::sort(cache.begin(), cache.end(), [this](int a, int b) {
      const float da = listItemDrawIndex(a);
      const float db = listItemDrawIndex(b);
      if (da != db)
      {
        return da < db;
      }
      return a < b;
    });
  }

  m_orderedListIndicesCacheValid = true;
}

const std::vector<int>& ArtboardComponentList::orderedListIndices()
{
  ensureOrderedListIndices();
  return m_cachedOrderedListIndices;
}

// A buffered row is realized just outside the scroll viewport, so it is
// mounted and advancing before it scrolls in. While the viewport's clip is on
// the renderer, a row that clips its own content can't show there, so drawing
// it is wasted. Only the clip on the renderer counts: a draw rule can draw the
// list outside its viewport's. Rows are measured in device space, which needs
// a renderer that reports its transform; without one every row draws. So does
// every row of a visited draw: the visitor can transform the list after the
// viewport clipped, and the renderer's transform then no longer places the
// clip. And so does every row a layer mask draws into its canvas: the
// viewport's clip may be on the frame's renderer instead, and the canvas is
// reused until the masked range changes, which the viewport isn't part of.
bool ArtboardComponentList::viewportCullsBufferedRows(
    Renderer* renderer,
    const Mat2D& listTransform,
    Mat2D* listToDevice,
    AABB* deviceViewport)
{
  if (artboard()->hasDrawVisitor() || artboard()->drawsMaskedRange())
  {
    return false;
  }
  auto scroll = scrollConstraint();
  if (scroll == nullptr || !scroll->hasLayoutParent())
  {
    return false;
  }
  auto viewport = scroll->content()->parent();
  if (viewport == nullptr || !viewport->is<LayoutComponent>() ||
      !viewport->as<LayoutComponent>()->clipsDrawing())
  {
    return false;
  }
  Mat2D toDevice;
  if (!renderer->currentTransform(&toDevice))
  {
    return false;
  }
  auto layout = viewport->as<LayoutComponent>();
  *listToDevice = toDevice * listTransform;
  *deviceViewport = (toDevice * layout->worldTransform())
                        .mapBoundingBox(layout->localBounds());
  return true;
}

// Whether a row clipped to its bounds stays clear of the viewport by more than
// antialiasing can bridge. A row drawn from its own bitmap cache composites a
// raster that can sample past its bounds, and one with its own rotation or
// scale clips in its rotated frame: both always draw.
static bool rowMissesViewport(
    ArtboardInstance* artboard,
    const Mat2D& rowToDevice,
    const AABB& viewport)
{
  if (!artboard->clip() || artboard->hasSelfTransform() ||
      artboard->bitmapCache() != nullptr)
  {
    return false;
  }
  constexpr float margin = 2.0f;
  const AABB row = rowToDevice.mapBoundingBox(artboard->bounds());
  return row.maxX + margin <= viewport.minX ||
         viewport.maxX + margin <= row.minX ||
         row.maxY + margin <= viewport.minY ||
         viewport.maxY + margin <= row.minY;
}

void ArtboardComponentList::draw(Renderer* renderer)
{
  // The rows keep their own opacity and get the list's here, so a fade
  // doesn't walk every component in every row. Content drawn from an
  // offscreen raster inside a row (a bitmap cache, a mask) fades as a group.
  const float opacity = renderOpacity();
  const bool modulates = opacity != 1.0f;
  const bool saves = needsSaveOperation() || modulates;
  if (saves)
  {
    renderer->save();
  }
  if (modulates)
  {
    renderer->modulateOpacity(opacity);
  }
  if (virtualizationEnabled())
  {
    const Mat2D& listTransform = parent()
                                     ->as<WorldTransformComponent>()
                                     ->worldTransform();
    Mat2D listToDevice;
    AABB deviceViewport;
    const bool culls = viewportCullsBufferedRows(
        renderer,
        listTransform,
        &listToDevice,
        &deviceViewport);
    renderer->transform(listTransform);
    if (!m_windowOrder.empty())
    {
      // We need to render in the correct order so we get the correct
      // z-index for items in cases where there is overlap
      for (int i : orderedListIndices())
      {
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
          auto transform = m_artboardTransforms[artboard];
          if (culls && !isWithinVisibleWindow(i) &&
              rowMissesViewport(
                  artboard,
                  listToDevice * transform,
                  deviceViewport))
          {
            artboard->skipDraw();
            continue;
          }
          renderer->save();
          renderer->transform(transform);
          Component::artboard()->drawHosted(artboard, renderer);
          renderer->restore();
        }
      }
    }
  }
  else
  {
    renderer->transform(worldTransform());
    for (int i : orderedListIndices())
    {
      auto artboard = artboardInstance(i);
      if (artboard != nullptr)
      {
        renderer->save();
        auto transform = m_artboardTransforms[artboard];
        renderer->transform(transform);
        Component::artboard()->drawHosted(artboard, renderer);
        renderer->restore();
      }
    }
  }
  if (saves)
  {
    renderer->restore();
  }
}

Core* ArtboardComponentList::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

bool ArtboardComponentList::hitTestHost(
    const Vec2D& position,
    bool skipOnUnclipped,
    ArtboardInstance* artboard)
{
  if (artboard == nullptr)
  {
    return false;
  }
  auto bounds = artboardPosition(artboard);
  Vec2D offset(bounds.x + position.x, bounds.y + position.y);
  auto transform = virtualizationEnabled()
                       ? parent()->as<LayoutComponent>()->worldTransform()
                       : worldTransform();
  return parent()->hitTestPoint(transform * offset, skipOnUnclipped, false);
}

Vec2D ArtboardComponentList::hostTransformPoint(
    const Vec2D& vec,
    ArtboardInstance* artboardInstance)
{
  auto bounds = artboardPosition(artboardInstance);
  Vec2D offset(bounds.x + vec.x, bounds.y + vec.y);
  auto transform = virtualizationEnabled()
                       ? parent()->as<LayoutComponent>()->worldTransform()
                       : worldTransform();
  auto localVec = transform * offset;
  auto ab = artboard();
  return ab ? ab->rootTransform(localVec) : localVec;
}

Mat2D ArtboardComponentList::worldTransformForArtboard(
    ArtboardInstance* artboardInstance)
{
  auto offset = artboardPosition(artboardInstance);
  // For scroll-into-view calculations, we need the position in content-local
  // space (without scroll applied). Use the list's layout position combined
  // with parent's world transform, rather than worldTransform() which may
  // include scroll constraints.
  auto* parentLayout = parent() != nullptr && parent()->is<LayoutComponent>()
                           ? parent()->as<LayoutComponent>()
                           : nullptr;
  if (parentLayout != nullptr)
  {
    AABB listBounds = layoutBounds();
    Mat2D transform = parentLayout->worldTransform() *
                      Mat2D::fromTranslate(listBounds.minX, listBounds.minY);
    return transform * Mat2D::fromTranslate(offset.x, offset.y);
  }
  auto transform = virtualizationEnabled()
                       ? parent()->as<LayoutComponent>()->worldTransform()
                       : worldTransform();
  return transform * Mat2D::fromTranslate(offset.x, offset.y);
}

void ArtboardComponentList::update(ComponentDirt value)
{
  Super::update(value);
  // The rows draw with the list's opacity (see draw) and only hear when it
  // turns fully transparent: content at 0 isn't hit, focused or selected,
  // and defers its path updates. Tracked with no rows too, for rows added
  // later (see addArtboardAt).
  bool hidesOrShowsRows = false;
  if (hasDirt(value, ComponentDirt::RenderOpacity))
  {
    const bool hidden = renderOpacity() <= 0.0f;
    hidesOrShowsRows = hidden != m_rowsHidden;
    m_rowsHidden = hidden;
  }
  if (artboardCount() == 0)
  {
    return;
  }

  if (hasDirt(value, ComponentDirt::WorldTransform))
  {
    // Mark semantic bounds dirty for nodes inside each list item
    // artboard. Their root-space bounds depend on the host's
    // world transform.
    forEachRealized([&](int, ArtboardInstance* artboard) {
      artboard->markSemanticBoundaryTransformDirty();
    });
  }
  if (hidesOrShowsRows)
  {
    const float hostOpacity = m_rowsHidden ? 0.0f : 1.0f;
    forEachRealized([&](int, ArtboardInstance* artboard) {
      artboard->hostOpacity(hostOpacity);
    });
  }
  // Rows resolve a host opacity change in this update, or they show a frame
  // late.
  if (hidesOrShowsRows || hasDirt(value, ComponentDirt::Components))
  {
    const size_t count = m_listItems.size();
    for (size_t row = nextAwakeRow(0); row < count; row = nextAwakeRow(row + 1))
    {
      auto artboard = m_artboardInstancesByIndex[row];
      if (artboard != nullptr)
      {
        artboard->updatePass(false);
      }
    }
#ifdef TESTING
    verifyQuietRows(RowPass::update, 0.0f, AdvanceFlags::None, false);
#endif
  }
}

void ArtboardComponentList::updateWorldTransform()
{
  updateArtboardsWorldTransform();
  Super::updateWorldTransform();
}

void ArtboardComponentList::updateArtboardsWorldTransform()
{
  auto count = m_listItems.size();
  if (count == 0)
  {
    return;
  }
  // We only update non layout transforms here
  if (!virtualizationEnabled())
  {
    auto useLayout = layoutParent() != nullptr;
    for (int i = 0; i < count; i++)
    {
      auto artboard = artboardInstance(i);
      if (artboard != nullptr)
      {
        auto bounds = useLayout ? artboard->layoutBounds()
                                : artboard->worldBounds();
        auto origin = useLayout ? artboard->origin() : Vec2D();
        m_artboardTransforms[artboard] = Mat2D::fromTranslate(
            bounds.left() - origin.x,
            bounds.top() - origin.y);
      }
    }
  }
}

void ArtboardComponentList::updateConstraints()
{
  if (layoutConstraints().size() > 0)
  {
    for (auto parentConstraint : layoutConstraints())
    {
      parentConstraint->constrainChild(this);
    }
  }
  if (m_listConstraints.size() > 0 && !virtualizationEnabled())
  {
    for (auto listConstraint : m_listConstraints)
    {
      listConstraint->constrainList(this);
    }
  }
  for (auto constraint : constraints())
  {
    auto listConstraint = ListConstraint::from(constraint);
    if (listConstraint != nullptr)
    {
      continue;
    }
    constraint->constrain(this);
  }
}

void ArtboardComponentList::internalDataContext(rcp<DataContext> value)
{
  // Reconcile the existing data contexts with the new parent. A row can have
  // no artboard (nothing bound to its view model) or no state machine (its
  // artboard has none), and both maps keep those rows as null entries.
  for (auto& artboard : m_artboardInstancesMap)
  {
    if (artboard.second == nullptr)
    {
      continue;
    }
    auto dataContext = artboard.second->dataContext();
    if (dataContext != nullptr)
    {
      dataContext->parent(value);
      artboard.second->internalDataContext(dataContext);
    }
  }
  for (auto& sm : m_stateMachinesMap)
  {
    if (sm.second == nullptr)
    {
      continue;
    }
    auto dataContext = sm.second->dataContext();
    if (dataContext != nullptr)
    {
      dataContext->parent(value);
      sm.second->internalDataContext(dataContext);
    }
  }
}

void ArtboardComponentList::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance,
    rcp<DataContext> parent)
{
  // At the time this is called, artboards will not yet have been instanced
  // so its essentially a no-op and the call to bindViewModelInstance on
  // each artboard is made at the time of artboard instancing
}

void ArtboardComponentList::clearDataContext() {}
void ArtboardComponentList::unbind()
{
  // The overrides size the rows they hold whenever their own sizes change,
  // so they let go of every row clear() destroys, pooled ones included. Not
  // in clear() itself: the destructor runs it after the artboard may have
  // deleted them.
  for (auto& child : children())
  {
    if (child->is<ArtboardComponentListOverride>())
    {
      child->as<ArtboardComponentListOverride>()->clearArtboards();
    }
  }
  clear();
}
void ArtboardComponentList::updateDataBinds()
{
  // A collapsed list does no bind work at all: not the rows' own binds, not
  // their state machines, and nothing mounted inside them. collapse() here
  // is semantic-only, exactly as in NestedArtboard, so nothing in a row is
  // ever flagged and DataBind::canSkip cannot see that the list is hidden --
  // without this, a toSource bind in a row publishes an uncomputed value
  // into the shared view model and every reader picks it up.
  if (isCollapsed())
  {
    return;
  }
  const size_t count = m_listItems.size();
  for (size_t row = nextAwakeRow(0); row < count; row = nextAwakeRow(row + 1))
  {
    auto stateMachine = m_stateMachinesByIndex[row];
    if (stateMachine != nullptr)
    {
      stateMachine->updateDataBinds(false);
    }
    auto artboard = m_artboardInstancesByIndex[row];
    if (artboard != nullptr)
    {
      artboard->updateDataBinds();
    }
  }
#ifdef TESTING
  verifyQuietRows(RowPass::updateDataBinds, 0.0f, AdvanceFlags::None, false);
#endif
}

#ifdef WITH_RIVE_TOOLS
Vec2D ArtboardComponentList::itemPosition(int index)
{
  auto artboard = artboardInstance(index);
  return artboard == nullptr ? Vec2D() : artboardPosition(artboard);
}
#endif

Vec2D ArtboardComponentList::artboardPosition(ArtboardInstance* artboard)
{
  auto mat = m_artboardTransforms[artboard];
  return Vec2D(mat[4], mat[5]);
}

bool ArtboardComponentList::worldToLocal(Vec2D world, Vec2D* local, int index)
{
  assert(local != nullptr);
  auto artboard = artboardInstance(index);
  if (artboard == nullptr)
  {
    return false;
  }
  Vec2D offset = artboardPosition(artboard);
  auto transform = virtualizationEnabled()
                       ? parent()->as<LayoutComponent>()->worldTransform()
                       : worldTransform();
  auto artboardTransform = transform * Mat2D::fromTranslate(offset.x, offset.y);
  Mat2D toMountedArtboard;
  if (!artboardTransform.invert(&toMountedArtboard))
  {
    return false;
  }

  *local = toMountedArtboard * world;

  return true;
}

void ArtboardComponentList::file(File* value) { m_file = value; }
File* ArtboardComponentList::file() const { return m_file; }

Core* ArtboardComponentList::clone() const
{
  auto clone = static_cast<ArtboardComponentList*>(
      ArtboardComponentListBase::clone());
  clone->file(file());
  return clone;
}

void ArtboardComponentList::createArtboardAt(int index, bool forceLayoutSync)
{
  auto item = listItem(index);
  if (item != nullptr)
  {
    auto artboardCopy = createArtboard(this, item);
    if (artboardCopy != nullptr)
    {
      attachArtboardOverride(artboardCopy.get());
      addArtboardAt(std::move(artboardCopy), index, forceLayoutSync);
    }
  }
}

void ArtboardComponentList::addArtboardAt(
    std::unique_ptr<ArtboardInstance> artboard,
    int index,
    bool forceLayoutSync,
    std::unique_ptr<StateMachineInstance> pooledStateMachine)
{
  auto item = listItem(index);
  if (item != nullptr)
  {
    auto artboardInstance = artboard.get();
    m_artboardInstancesMap[item] = std::move(artboard);
    // In a virtualized list, the rows showing the item get each new
    // instance right away: hosting it and syncing the layout below look it
    // up by row. A list can show one item on several rows, and those rows
    // share its instances.
    const bool virtualized = virtualizationEnabled();
    if (virtualized)
    {
      setRowsForItem(index, item, artboardInstance, nullptr);
    }
    bindArtboard(artboardInstance, item);
    if (artboardInstance != nullptr)
    {
      artboardInstance->host(this);
      // A pooled row may still carry the opacity it last had here.
      artboardInstance->hostOpacity(m_rowsHidden ? 0.0f : 1.0f);
      artboardInstance->frameOrigin(false);
      artboardInstance->parentIsRow(mainAxisIsRow());
      artboardInstance->parentIsStack(isStack());
#ifdef WITH_RIVE_LAYOUT
      // Cascade into the new row what its layout parent cascades into the
      // rows it already has: its layout animation and direction. The parent
      // only cascades when its own style changes, so a row added later would
      // otherwise never inherit them. Doing it now, before the row's first
      // layout solve, also means a row with a right-to-left layout doesn't
      // flip its children after that solve and have the whole host solved
      // again. Without a layout parent the row cascades its own style, as
      // its update would a step later.
      LayoutComponent* from = layoutParent();
      if (from == nullptr)
      {
        from = artboardInstance;
      }
      artboardInstance->cascadeLayoutStyle(
          from->interpolation(),
          from->interpolator(),
          from->interpolationTime(),
          from->actualDirection());
#endif
    }

    StateMachineInstance* stateMachineInstance = nullptr;
    if (pooledStateMachine != nullptr)
    {
      auto sm = pooledStateMachine.get();
      sm->resetState();
      applyRecorders(sm, findArtboard(item));
      m_stateMachinesMap[item] = std::move(pooledStateMachine);
      if (virtualized)
      {
        setRowsForItem(index, item, artboardInstance, sm);
      }
      linkStateMachineToArtboard(sm, artboardInstance);
      stateMachineInstance = sm;
      // Back in the tree without being rebuilt, so a root that cached its
      // tree as capture-free has to look again.
      if (sm->hasCaptureListeners())
      {
        StateMachineInstance::captureTopologyChanged();
      }
    }
    if (stateMachineInstance == nullptr)
    {
      auto stateMachineCopy = createStateMachineInstance(
          this,
          artboardInstance);
      stateMachineInstance = stateMachineCopy.get();
      m_stateMachinesMap[item] = std::move(stateMachineCopy);
      if (virtualized)
      {
        setRowsForItem(index, item, artboardInstance, stateMachineInstance);
      }
    }

    if (!virtualized)
    {
      // updateList builds a non-virtualized list's rows in order and
      // gives an item's later rows the instances made for its first.
      resetQuietRow(index);
      placeInRow(index, artboardInstance);
      m_stateMachinesByIndex[index] = stateMachineInstance;
      markRealized(index, artboardInstance != nullptr);
    }
    // After the instance is findable by index, which the sync reads.
    if (forceLayoutSync)
    {
      syncLayoutChildren();
    }
  }
}

void ArtboardComponentList::bindArtboard(
    ArtboardInstance* artboardInstance,
    rcp<ViewModelInstanceListItem> listItem)
{
  if (artboardInstance != nullptr)
  {
    auto mainArtboard = this->artboard();
    auto dataContext = mainArtboard->dataContext();
    rcp<ViewModelInstance> viewModelInstance = listItem->viewModelInstance();

    // TODO: @hernan added this to make sure data binds are procesed in the
    // current frame instead of waiting for the next run. But might not be
    // necessary. Needs more testing.
    artboardInstance->bindViewModelInstance(viewModelInstance, dataContext);
    artboardInstance->updateDataBinds();

    invalidateOrderedListIndicesCache();
  }
}

void ArtboardComponentList::removeArtboardAt(int index)
{
  auto item = listItem(index);
  // The rows let go of the item's instances before they are destroyed.
  setRowsForItem(index, item, nullptr, nullptr);
  removeArtboard(item);
}

void ArtboardComponentList::removeArtboard(rcp<ViewModelInstanceListItem> item)
{
  invalidateOrderedListIndicesCache();
  removeDrawIndexListenerForItem(item);
  auto itr = m_artboardInstancesMap.find(item);
  if (itr != m_artboardInstancesMap.end())
  {
    // Clean up semantic tree before destroying the artboard.
    if (itr->second != nullptr)
    {
      itr->second->cleanupSemanticTree();
    }
    // Clean up focus tree before destroying the artboard to prevent
    // use-after-free when the FocusManager still holds references
    // to FocusNodes pointing to FocusData in this artboard.
    if (itr->second != nullptr)
    {
      itr->second->cleanupFocusTree();
    }
    clearArtboardOverride(itr->second.get());
  }
  // Destroy state machines BEFORE artboards to ensure FocusListenerGroup
  // can unregister from FocusData before the artboard (and its FocusData)
  // is destroyed. Otherwise we get use-after-free in ~FocusListenerGroup.
  m_stateMachinesMap.erase(item);
  m_artboardInstancesMap.erase(item);
}

void ArtboardComponentList::createArtboardRecorders(const Artboard* artboard)
{
  if (artboard == nullptr)
  {
    return;
  }
  auto recorderIt = m_propertyRecordersMap.find(artboard);
  if (recorderIt == m_propertyRecordersMap.end())
  {
    auto propertyRecorder = std::make_unique<PropertyRecorder>();
    propertyRecorder->recordArtboard(artboard);
    m_propertyRecordersMap[artboard] = std::move(propertyRecorder);
    for (auto& nestedArtboard : artboard->nestedArtboards())
    {
      if (nestedArtboard == nullptr)
      {
        continue;
      }
      auto sourceArtboard = nestedArtboard->sourceArtboard();
      createArtboardRecorders(sourceArtboard);
    }
  }
}

void ArtboardComponentList::applyRecorders(
    Artboard* artboard,
    const Artboard* sourceArtboard)
{
  if (artboard == nullptr)
  {
    return;
  }
  auto sourcedArtboardIt = m_propertyRecordersMap.find(sourceArtboard);
  if (sourcedArtboardIt != m_propertyRecordersMap.end())
  {
    auto propertyRecorder = sourcedArtboardIt->second.get();
    if (propertyRecorder != nullptr)
    {
      propertyRecorder->apply(artboard);
    }
  }
  for (auto& nestedArtboard : artboard->nestedArtboards())
  {
    if (nestedArtboard == nullptr)
    {
      continue;
    }
    auto nestedInstance = nestedArtboard->sourceArtboard();
    if (nestedInstance == nullptr)
    {
      continue;
    }
    applyRecorders(nestedInstance, nestedInstance->artboardSource());
  }
}

void ArtboardComponentList::applyRecorders(
    StateMachineInstance* stateMachineInstance,
    const Artboard* sourceArtboard)
{
  auto it = m_propertyRecordersMap.find(sourceArtboard);
  if (it == m_propertyRecordersMap.end())
  {
    return;
  }
  auto propertyRecorder = it->second.get();
  if (propertyRecorder != nullptr)
  {
    propertyRecorder->apply(stateMachineInstance);
  }
}

void ArtboardComponentList::addVirtualizable(int index)
{
  auto listItem = this->listItem(index);
  if (listItem != nullptr)
  {
    auto artboard = findArtboard(listItem);
    if (artboard != nullptr && !refusesNestedRow(artboard))
    {
      RowBuildScope build(artboard, m_listSource, this);
      createArtboardRecorders(artboard);
      auto& pool = m_rowPool[artboard];
      if (pool.empty())
      {
        createArtboardAt(index);
      }
      else
      {
        auto pooled = std::move(pool.back());
        pool.pop_back();
        applyRecorders(pooled.artboard.get(), artboard);
        addArtboardAt(
            std::move(pooled.artboard),
            index,
            true,
            std::move(pooled.stateMachine));
      }
      // Add components dirt so we process layout updates in the same
      // frame that a mounted artboard is added
      addDirt(ComponentDirt::Components);
      auto p = layoutParent();
      if (p != nullptr)
      {
        p->markLayoutStyleDirty();
      }
    }
  }
}

void ArtboardComponentList::virtualizableChanged()
{
  if (m_updatingList)
  {
    return;
  }
  auto* parentArtboard = this->artboard();
  if (parentArtboard != nullptr && parentArtboard->focusManager() != nullptr)
  {
    auto* parentFM = parentArtboard->focusManager();
    if (m_listScopeFocusNode != nullptr)
    {
      syncListRowNodesWithList(parentFM);
    }
  }
}

void ArtboardComponentList::notePointer(int pointerId, Vec2D position)
{
  m_notedPointers.eraseIf(
      [pointerId](const NotedPointer& noted) { return noted.id == pointerId; });
  m_notedPointers.push_back({pointerId, position});
}

void ArtboardComponentList::forgetPointer(int pointerId)
{
  m_notedPointers.eraseIf(
      [pointerId](const NotedPointer& noted) { return noted.id == pointerId; });
}

void ArtboardComponentList::exitNotedPointers(int index)
{
  auto stateMachine = stateMachineInstance(index);
  if (stateMachine == nullptr || m_notedPointers.empty())
  {
    return;
  }
  // A copy: the exit listeners may do anything.
  const std::vector<NotedPointer> pointers = m_notedPointers.view();
  for (const auto& pointer : pointers)
  {
    // Where the pointer now is relative to the row, as the next event would
    // have put it, or where it was if the row can't map it.
    Vec2D local = pointer.position;
    worldToLocal(pointer.position, &local, index);
    // Whatever turned the pointer off for the row (a drag scrolling the
    // list, a claim) can't turn it back on once the row is gone, and a
    // listener that is off misses its exit.
    stateMachine->dropPointer(local, pointer.id);
  }
  // What the exits reported would otherwise wait for an advance: one the
  // row doesn't get once destroyed, or gets bound to another item once
  // reused.
  stateMachine->applyPendingReports();
}

void ArtboardComponentList::exitRowsLeaving(
    const std::vector<rcp<ViewModelInstanceListItem>>& list)
{
  const auto staying = sortedItems(list);
  // Rows showing the same item share its machine, which leaves once.
  std::vector<StateMachineInstance*> exited;
  for (size_t i = 0; i < m_listItems.size(); i++)
  {
    auto stateMachine = stateMachineInstance((int)i);
    if (stateMachine == nullptr || std::binary_search(
                                       staying.begin(),
                                       staying.end(),
                                       m_listItems[i].get()))
    {
      continue;
    }
    if (m_listHasDuplicateItems)
    {
      if (std::find(exited.begin(), exited.end(), stateMachine) != exited.end())
      {
        continue;
      }
      exited.push_back(stateMachine);
    }
    exitNotedPointers((int)i);
  }
}

void ArtboardComponentList::removeVirtualizable(int index)
{
  auto listItem = this->listItem(index);
  if (listItem != nullptr)
  {
    exitNotedPointers(index);
    // The rows let go of the item's instances before they move to the
    // pool.
    setRowsForItem(index, listItem, nullptr, nullptr);
    auto artboard = findArtboard(listItem);
    auto instanceItr = m_artboardInstancesMap.find(listItem);
    // A row whose item no longer maps to an artboard is pooled all the same,
    // under no artboard, where nothing takes it: no row can be destroyed
    // here. This runs in the scroll pass, and the layout parent holds the
    // row's layout node until it next syncs its children.
    if (instanceItr != m_artboardInstancesMap.end() &&
        instanceItr->second != nullptr)
    {
      // A pooled instance may be reused anywhere, so it keeps no cell.
      instanceItr->second->virtualGridCell(-1, -1);
      PooledRow pooled;
      pooled.artboard = std::move(instanceItr->second);
      auto smItr = m_stateMachinesMap.find(listItem);
      if (smItr != m_stateMachinesMap.end())
      {
        pooled.stateMachine = std::move(smItr->second);
      }
      m_rowPool[artboard].push_back(std::move(pooled));
    }
  }
  removeArtboardAt(index);
}

void ArtboardComponentList::realizedIndices(std::vector<int>& out)
{
  out.insert(out.end(), m_realizedIndices.begin(), m_realizedIndices.end());
}

void ArtboardComponentList::markRealized(size_t index, bool realized)
{
  auto it = std::lower_bound(
      m_realizedIndices.begin(),
      m_realizedIndices.end(),
      (int)index);
  bool present = it != m_realizedIndices.end() && *it == (int)index;
  if (realized && !present)
  {
    m_realizedIndices.insert(it, (int)index);
  }
  else if (!realized && present)
  {
    m_realizedIndices.erase(it);
  }
}

void ArtboardComponentList::setVirtualizableCell(int index, int column, int row)
{
  auto artboard = artboardInstance(index);
  if (artboard != nullptr)
  {
    artboard->virtualGridCell(column, row);
  }
}

void ArtboardComponentList::setVirtualizablePosition(int index, Vec2D position)
{
  auto artboard = this->artboardInstance(index);
  if (artboard != nullptr)
  {
    auto useLayout = layoutParent() != nullptr;
    auto origin = useLayout ? artboard->origin() : Vec2D();
    m_artboardTransforms[artboard] = Mat2D::fromTranslate(
        position.x - origin.x,
        position.y - origin.y);
  }
}

bool ArtboardComponentList::virtualizationEnabled()
{
  auto virtualizer = scrollConstraint();
  return virtualizer != nullptr && virtualizer->virtualize();
}

ScrollConstraint* ArtboardComponentList::scrollConstraint()
{
  for (auto parentConstraint : layoutConstraints())
  {
    if (parentConstraint->constraint()->is<ScrollConstraint>())
    {
      return parentConstraint->constraint()->as<ScrollConstraint>();
    }
  }
  return nullptr;
}

void ArtboardComponentList::computeLayoutBounds()
{
  if (virtualizationEnabled())
  {
    auto gap = this->gap();
    bool isHorz = mainAxisIsRow();
    if (m_layoutSizeVersion != m_itemsVersion || m_layoutSizeGap != gap ||
        m_layoutSizeIsHorz != isHorz)
    {
      m_layoutSizeVersion = m_itemsVersion;
      m_layoutSizeGap = gap;
      m_layoutSizeIsHorz = isHorz;
      auto runningWidth = 0.0f;
      auto runningHeight = 0.0f;
      for (int i = 0; i < m_artboardSizes.size(); i++)
      {
        auto size = m_artboardSizes[i];
        auto realGap = i == m_artboardSizes.size() - 1 ? 0 : gap;
        if (isHorz)
        {
          runningWidth += size.x + realGap;
          runningHeight = std::max(runningHeight, size.y);
        }
        else
        {
          runningWidth = std::max(runningWidth, size.x);
          runningHeight += size.y + realGap;
        }
      }
      m_layoutSize = Vec2D(runningWidth, runningHeight);
    }

    auto scroll = scrollConstraint();
    if (scroll != nullptr)
    {
      scroll->constrainVirtualized(true);
    }
  }
}

Vec2D ArtboardComponentList::size() { return m_layoutSize; }

Vec2D ArtboardComponentList::itemSize(int index)
{
  return index < m_artboardSizes.size() ? m_artboardSizes[index] : Vec2D();
}

void ArtboardComponentList::setItemSize(Vec2D size, int index)
{
  if (index < m_artboardSizes.size() && m_artboardSizes[index] != size)
  {
    m_artboardSizes[index] = size;
    m_itemsVersion = ++s_itemsVersion;
  }
}

float ArtboardComponentList::gap()
{
  auto p = layoutParent();
  if (p != nullptr)
  {
    return mainAxisIsRow() ? p->gapHorizontal() : p->gapVertical();
  }
  return 0.0f;
}

void ArtboardComponentList::attachArtboardOverride(ArtboardInstance* instance)
{
  if (instance == nullptr)
  {
    return;
  }
  // An override names the artboard it applies to by its index in the file,
  // as a map rule does: the artboard the row is an instance of, which a map
  // rule may have picked over the first one bound to the item's view model.
  // A row of none of them only takes an override for every artboard.
  int artboardIndex = -1;
  auto source = instance->artboardSource();
  for (size_t i = 0, count = m_file->artboardCount(); i < count; i++)
  {
    // Without decoding the artboards before it.
    if (m_file->artboardHeader(i)->handsOut(source))
    {
      artboardIndex = static_cast<int>(i);
      break;
    }
  }
  ArtboardComponentListOverride* artboardOverride = nullptr;
  for (auto& child : children())
  {
    if (child->is<ArtboardComponentListOverride>())
    {
      if (child->as<ArtboardComponentListOverride>()->artboardId() == -1)
      {
        artboardOverride = child->as<ArtboardComponentListOverride>();
      }
      else if (
          child->as<ArtboardComponentListOverride>()->artboardId() ==
          artboardIndex)
      {
        artboardOverride = child->as<ArtboardComponentListOverride>();
        break;
      }
    }
  }
  if (artboardOverride != nullptr)
  {
    artboardOverride->addArtboard(instance);
  }
}

void ArtboardComponentList::clearArtboardOverride(
    ArtboardInstance* artboardInstance)
{
  for (auto& child : children())
  {
    if (child->is<ArtboardComponentListOverride>())
    {
      // Passing the artboard to all overrides in case in the future we
      // support stacking overrides on top of each other.
      // Currently all except one will be a no-op.
      child->as<ArtboardComponentListOverride>()->removeArtboard(
          artboardInstance);
    }
  }
}

bool ArtboardComponentList::mainAxisIsRow()
{
  auto p = layoutParent();
  return p != nullptr ? p->mainAxisIsRow() : true;
}

bool ArtboardComponentList::isStack()
{
  auto p = layoutParent();
  return p != nullptr && p->isStackContainer();
}

LayoutComponent* ArtboardComponentList::layoutParent()
{
  auto* direct = parent();
  if (direct == nullptr)
  {
    return nullptr;
  }
  if (direct->is<LayoutComponent>())
  {
    return direct->as<LayoutComponent>();
  }
  if (!joinsLayoutThroughContainer(this))
  {
    return nullptr;
  }
  for (auto* p = direct; p != nullptr; p = p->parent())
  {
    if (p->is<LayoutComponent>())
    {
      return p->as<LayoutComponent>();
    }
    if (!isTransparentLayoutContainer(p))
    {
      return nullptr;
    }
  }
  return nullptr;
}

const Mat2D& ArtboardComponentList::listTransform() { return worldTransform(); }

void ArtboardComponentList::listItemTransforms(std::vector<Mat2D*>& transforms)
{
  auto count = m_listItems.size();
  for (int i = 0; i < count; i++)
  {
    auto artboard = artboardInstance(i);
    if (artboard != nullptr)
    {
      transforms.push_back(&m_artboardTransforms[artboard]);
    }
  }
}

void ArtboardComponentList::addMapRule(ArtboardListMapRule* rule)
{
  m_artboardMapRules[rule->viewModelId()] = rule->artboardId();
}
#ifdef WITH_RIVE_EDITOR
void ArtboardComponentList::editorPrepareForDestruction()
{
  clear();
  Component::editorPrepareForDestruction();
}

void ArtboardComponentList::forEachHostedArtboard(
    const std::function<void(ArtboardInstance*)>& visit)
{
  for (auto& item : m_artboardInstancesMap)
  {
    if (item.second != nullptr)
    {
      visit(item.second.get());
    }
  }
  for (auto& pool : m_rowPool)
  {
    for (auto& row : pool.second)
    {
      if (row.artboard != nullptr)
      {
        visit(row.artboard.get());
      }
    }
  }
}

#endif
