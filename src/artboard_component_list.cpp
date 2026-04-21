#include "rive/component.hpp"
#include "rive/file.hpp"
#include <algorithm>
#include <cmath>
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/list_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/data_bind/data_bind.hpp"
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

using namespace rive;

namespace rive
{
/// Marks the hosting list dirty when a list item's drawIndex VM value changes.
class ArtboardListDrawIndexDependent final : public ViewModelValueDependent
{
public:
    ArtboardListDrawIndexDependent(ArtboardComponentList* list,
                                   ViewModelInstanceValue* value) :
        m_list(list), m_value(ref_rcp(value))
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

ArtboardComponentList::ArtboardComponentList() {}
ArtboardComponentList::~ArtboardComponentList() { clear(); }

void ArtboardComponentList::clear()
{
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

    // Clean up bridge binds FIRST since they reference VM instances
    // that may be owned by the artboard instances below.
    auto* parentAb = artboard();
    for (auto& [item, binds] : m_bridgeDataBinds)
    {
        for (auto& bind : binds)
        {
            bind->unbind();
            if (parentAb != nullptr)
            {
                parentAb->removeDataBind(bind.get());
            }
        }
    }
    m_bridgeDataBinds.clear();

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
    m_stateMachinesByIndex.clear();
    m_artboardInstancesMap.clear();
    m_listItems.clear();
    m_artboardsMap.clear();
    m_resourcePool.clear();
    m_stateMachinesPool.clear();
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
    if (!virtualizationEnabled())
    {
        if (index >= 0 && index < m_artboardInstancesByIndex.size())
        {
            return m_artboardInstancesByIndex[index];
        }
        return nullptr;
    }
    if (index >= 0 && index < m_listItems.size())
    {
        auto item = listItem(index);
        auto itr = m_artboardInstancesMap.find(item);
        if (itr != m_artboardInstancesMap.end())
        {
            return itr->second.get();
        }
    }
    return nullptr;
}
StateMachineInstance* ArtboardComponentList::stateMachineInstance(int index)
{
    if (!virtualizationEnabled())
    {
        if (index >= 0 && index < m_stateMachinesByIndex.size())
        {
            return m_stateMachinesByIndex[index];
        }
        return nullptr;
    }
    if (index >= 0 && index < m_listItems.size())
    {
        auto item = listItem(index);
        auto itr = m_stateMachinesMap.find(item);
        if (itr != m_stateMachinesMap.end())
        {
            return itr->second.get();
        }
    }
    return nullptr;
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
#endif

void ArtboardComponentList::markLayoutNodeDirty(
    bool shouldForceUpdateLayoutBounds)
{
    bool parentIsRow = mainAxisIsRow();
    for (int i = 0; i < artboardCount(); i++)
    {
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
            artboard->parentIsRow(parentIsRow);
        }
    }
}

void ArtboardComponentList::updateLayoutBounds(bool animate)
{
#ifdef WITH_RIVE_LAYOUT
    for (int i = 0; i < artboardCount(); i++)
    {
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
            artboard->updateLayoutBounds(animate);
            auto bounds = artboard->layoutBounds();
            setItemSize(Vec2D(bounds.width(), bounds.height()), i);
        }
    }
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
    for (int i = 0; i < (int)artboardCount(); i++)
    {
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
            artboard->cascadeLayoutStyle(inheritedInterpolation,
                                         inheritedInterpolator,
                                         inheritedInterpolationTime,
                                         direction);
        }
    }
    return false;
}
#endif

bool ArtboardComponentList::syncStyleChanges()
{
    bool changed = false;
    for (int i = 0; i < artboardCount(); i++)
    {
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
            bool artboardChanged = artboard->syncStyleChanges();
            if (artboardChanged)
            {
                changed = true;
            }
        }
    }
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
        auto stateMachineInstance = artboard->stateMachineAt(0);
        linkStateMachineToArtboard(stateMachineInstance.get(), artboard);
        return stateMachineInstance;
    }
    return nullptr;
}

void ArtboardComponentList::linkStateMachineToArtboard(
    StateMachineInstance* stateMachineInstance,
    ArtboardInstance* artboardInstance)
{
    if (artboardInstance != nullptr && stateMachineInstance != nullptr)
    {
        auto dataContext = artboardInstance->dataContext();
        stateMachineInstance->dataContext(dataContext);
        // TODO: @hernan added this to make sure data binds are procesed in the
        // current frame instead of waiting for the next run. But might not be
        // necessary. Needs more testing.
        stateMachineInstance->updateDataBinds(false);

        // Share parent artboard's focus manager and build focus tree for list
        // item.
        auto* parentArtboard = this->artboard();
        if (parentArtboard != nullptr &&
            parentArtboard->focusManager() != nullptr)
        {
            auto* parentFM = parentArtboard->focusManager();
            stateMachineInstance->setExternalFocusManager(parentFM);

            // Find closest focus node (handles artboard boundaries)
            auto parentNode = FocusData::findClosestFocusNode(this);

            // Build list item's focus tree under parent
            artboardInstance->buildFocusTree(parentFM, parentNode);
        }
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
    m_oldItems.clear();
    m_oldItems.assign(m_listItems.begin(), m_listItems.end());
    m_listItems.clear();
    m_listItems.assign(list->begin(), list->end());
    invalidateOrderedListIndicesCache();
    m_artboardSizes.clear();

    // Clear the index vectors - they'll be rebuilt as artboards are created
    m_artboardInstancesByIndex.clear();
    m_stateMachinesByIndex.clear();
    if (!virtualizationEnabled())
    {
        m_artboardInstancesByIndex.resize(m_listItems.size(), nullptr);
        m_stateMachinesByIndex.resize(m_listItems.size(), nullptr);
    }

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
    for (auto item : m_oldItems)
    {
        auto it = std::find(m_listItems.begin(), m_listItems.end(), item);
        if (it == m_listItems.end())
        {
            disposeListItem(item);
        }
    }
    uint32_t index = 0;
    for (auto& item : m_listItems)
    {
        auto viewModelInstance = item->viewModelInstance();
        if (viewModelInstance != nullptr)
        {
            auto symbol = viewModelInstance->symbol(
                ViewModelInstanceSymbolListIndexBase::typeKey);
            if (symbol != nullptr)
            {
                symbol->as<ViewModelInstanceSymbolListIndex>()->propertyValue(
                    index);
            }
        }
        auto artboard = findArtboard(item);
        if (artboard != nullptr)
        {
            m_artboardSizes.push_back(
                Vec2D(artboard->width(), artboard->height()));
        }
        auto itr = m_artboardInstancesMap.find(item);
        if (!virtualizationEnabled())
        {
            if (itr == m_artboardInstancesMap.end())
            {
                createArtboardAt(index, false);
            }
            else
            {
                // Existing artboard - update index vectors
                m_artboardInstancesByIndex[index] = itr->second.get();
                auto smItr = m_stateMachinesMap.find(item);
                if (smItr != m_stateMachinesMap.end())
                {
                    m_stateMachinesByIndex[index] = smItr->second.get();
                }
            }
        }
        index++;
    }
    computeLayoutBounds();
    syncLayoutChildren();
    markLayoutNodeDirty();
    markWorldTransformDirty();
    addDirt(ComponentDirt::Components);
    recomputeListUsesDrawIndexSort();
    syncDrawIndexListeners();
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

bool ArtboardComponentList::advanceComponent(float elapsedSeconds,
                                             AdvanceFlags flags)
{
    if (artboardCount() == 0 || isCollapsed())
    {
        return false;
    }
    bool keepGoing = false;
    bool advanceNested =
        (flags & AdvanceFlags::AdvanceNested) == AdvanceFlags::AdvanceNested;
    bool newFrame = (flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame;
    auto advancingFlags = flags & ~AdvanceFlags::IsRoot;
    for (int i = 0; i < artboardCount(); i++)
    {
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

    return keepGoing;
}

void ArtboardComponentList::reset()
{
    for (auto& item : m_listItems)
    {
        auto itr = m_artboardInstancesMap.find(item);
        if (m_shouldResetInstances)
        {
            auto viewModelInstance = item->viewModelInstance();
            if (viewModelInstance != nullptr)
            {
                viewModelInstance->advanced();
            }
            if (itr != m_artboardInstancesMap.end() && itr->second != nullptr)
            {
                auto dataContext = itr->second->dataContext();
                if (dataContext != nullptr)
                {
                    auto boundInstance = dataContext->viewModelInstance();
                    if (boundInstance != nullptr &&
                        boundInstance != viewModelInstance)
                    {
                        boundInstance->advanced();
                    }
                }
            }
        }
        if (itr != m_artboardInstancesMap.end())
        {
            itr->second->reset();
        }
    }
}

AABB ArtboardComponentList::layoutBounds()
{
    return AABB(0, 0, m_layoutSize.x, m_layoutSize.y);
}

AABB ArtboardComponentList::layoutBoundsForNode(int index)
{
    if (virtualizationEnabled())
    {
        auto realIndex = std::fmod(index, m_listItems.size());
        auto gap = this->gap();
        float runningSize = 0;
        bool isHorizontal = mainAxisIsRow();
        for (int i = 0; i < realIndex; i++)
        {
            auto size = m_artboardSizes[i];
            if (isHorizontal)
            {
                runningSize += size.x + gap;
            }
            else
            {
                runningSize += size.y + gap;
            }
        }
        auto itemSize = m_artboardSizes[realIndex];
        double left = isHorizontal ? runningSize : 0;
        double top = isHorizontal ? 0 : runningSize;
        return AABB(left, top, left + itemSize.x, top + itemSize.y);
    }
    else
    {
        if (index >= 0 && index < numLayoutNodes())
        {
            return artboardInstance(index)->layoutBounds();
        }
    }
    return AABB();
}

void ArtboardComponentList::markHostingLayoutDirty(
    ArtboardInstance* artboardInstance)
{
    // TODO: Should optimize this
    for (int i = 0; i < artboardCount(); i++)
    {
        auto artboard = this->artboardInstance(i);
        if (artboard != nullptr && artboard == artboardInstance)
        {
            this->artboard()->markLayoutDirty(artboardInstance);
            break;
        }
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
    auto* vmi =
        m_listItems[static_cast<size_t>(index)]->viewModelInstance().get();
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
        m_drawIndexDependents[item] =
            std::make_unique<ArtboardListDrawIndexDependent>(this, prop);
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
                                  m_visibleStartIndex >= 0 &&
                                  m_visibleEndIndex >= 0;

    if (useVirtualWindow)
    {
        auto startIndex = m_visibleStartIndex % count;
        auto endIndex = m_visibleEndIndex % count;
        int i = startIndex;
        while (true)
        {
            cache.push_back(i);
            if (i == endIndex)
            {
                break;
            }
            i = (i + 1) % count;
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

void ArtboardComponentList::draw(Renderer* renderer)
{
    if (m_needsSaveOperation)
    {
        renderer->save();
    }
    if (virtualizationEnabled())
    {
        renderer->transform(
            parent()->as<WorldTransformComponent>()->worldTransform());
        if (m_visibleStartIndex != -1 && m_visibleEndIndex != -1)
        {
            // We need to render in the correct order so we get the correct
            // z-index for items in cases where there is overlap
            for (int i : orderedListIndices())
            {
                auto artboard = artboardInstance(i);
                if (artboard != nullptr)
                {
                    renderer->save();
                    auto transform = m_artboardTransforms[artboard];
                    renderer->transform(transform);
                    artboard->drawInternal(renderer);
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
                artboard->drawInternal(renderer);
                renderer->restore();
            }
        }
    }
    if (m_needsSaveOperation)
    {
        renderer->restore();
    }
}

Core* ArtboardComponentList::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

bool ArtboardComponentList::hitTestHost(const Vec2D& position,
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
        Mat2D transform =
            parentLayout->worldTransform() *
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
    if (artboardCount() == 0)
    {
        return;
    }

    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        for (int i = 0; i < artboardCount(); i++)
        {
            auto artboard = artboardInstance(i);
            if (artboard != nullptr)
            {
                artboard->opacity(renderOpacity());
            }
        }
    }
    if (hasDirt(value, ComponentDirt::Components))
    {
        for (int i = 0; i < artboardCount(); i++)
        {
            auto artboard = artboardInstance(i);
            if (artboard != nullptr)
            {
                artboard->updatePass(false);
            }
        }
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
                m_artboardTransforms[artboard] =
                    Mat2D::fromTranslate(bounds.left() - origin.x,
                                         bounds.top() - origin.y);
            }
        }
    }
}

void ArtboardComponentList::updateConstraints()
{
    if (m_layoutConstraints.size() > 0)
    {
        for (auto parentConstraint : m_layoutConstraints)
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
    // Reconcile the existing data contexts with the new parent
    for (auto& artboard : m_artboardInstancesMap)
    {
        auto dataContext = artboard.second->dataContext();
        if (dataContext != nullptr)
        {
            dataContext->parent(value);
            artboard.second->internalDataContext(dataContext);
        }
    }
    for (auto& sm : m_stateMachinesMap)
    {
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
void ArtboardComponentList::unbind() { clear(); }
void ArtboardComponentList::updateDataBinds()
{
    for (int i = 0; i < artboardCount(); i++)
    {
        auto stateMachine = stateMachineInstance(i);
        if (stateMachine != nullptr)
        {
            stateMachine->updateDataBinds(false);
        }
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
            artboard->updateDataBinds();
        }
    }
}

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
    auto artboardTransform =
        transform * Mat2D::fromTranslate(offset.x, offset.y);
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
    auto clone =
        static_cast<ArtboardComponentList*>(ArtboardComponentListBase::clone());
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
            attachArtboardOverride(artboardCopy.get(), item);
            addArtboardAt(std::move(artboardCopy), index, forceLayoutSync);
        }
    }
}

void ArtboardComponentList::addArtboardAt(
    std::unique_ptr<ArtboardInstance> artboard,
    int index,
    bool forceLayoutSync)
{
    auto item = listItem(index);
    if (item != nullptr)
    {
        auto artboardInstance = artboard.get();
        m_artboardInstancesMap[item] = std::move(artboard);
        bindArtboard(artboardInstance, item);
        if (artboardInstance != nullptr)
        {
            artboardInstance->host(this);
            artboardInstance->frameOrigin(false);
            artboardInstance->parentIsRow(mainAxisIsRow());
        }
        if (forceLayoutSync)
        {
            syncLayoutChildren();
        }

        StateMachineInstance* stateMachineInstance = nullptr;
        auto artboard = findArtboard(item);
        if (artboard != nullptr)
        {

            auto& smPool = m_stateMachinesPool[artboard];
            if (!smPool.empty())
            {
                auto sm = smPool.back().get();

                sm->resetState();
                applyRecorders(sm, artboard);
                m_stateMachinesMap[item] = std::move(smPool.back());
                linkStateMachineToArtboard(sm, artboardInstance);
                stateMachineInstance = sm;
                smPool.pop_back();
            }
        }
        if (stateMachineInstance == nullptr)
        {
            auto stateMachineCopy =
                createStateMachineInstance(this, artboardInstance);
            stateMachineInstance = stateMachineCopy.get();
            m_stateMachinesMap[item] = std::move(stateMachineCopy);
        }

        if (!virtualizationEnabled())
        {
            if (index >= m_artboardInstancesByIndex.size())
            {
                m_artboardInstancesByIndex.resize(index + 1, nullptr);
                m_stateMachinesByIndex.resize(index + 1, nullptr);
            }
            m_artboardInstancesByIndex[index] = artboardInstance;
            m_stateMachinesByIndex[index] = stateMachineInstance;
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
        rcp<ViewModelInstance> viewModelInstance = nullptr;

        // Check if the source artboard is stateful - if so, create a new
        // instance for it (takes priority over any existing list item
        // instance). Clone the list item's instance when available so we
        // pick up its property values; otherwise fall back to the default.
        if (m_file != nullptr)
        {
            auto source = artboardInstance->artboardSource();
            if (source != nullptr && source->isStateful())
            {
                auto listItemInstance = listItem->viewModelInstance();
                if (listItemInstance != nullptr)
                {
                    auto copy = rcp<ViewModelInstance>(
                        listItemInstance->clone()->as<ViewModelInstance>());
                    m_file->completeViewModelInstance(copy);
#ifdef WITH_RIVE_TOOLS
                    if (copy)
                    {
                        m_file->registerViewModelInstance(copy.get(), copy);
                    }
#endif
                    viewModelInstance = copy;

                    // Create bridge data binds between the original and
                    // cloned VM instances for input/output properties.
                    createBridgeBinds(listItem,
                                      listItemInstance.get(),
                                      copy.get());
                }
                else
                {
                    auto viewModel = m_file->viewModel(source->viewModelId());
                    if (viewModel != nullptr)
                    {
                        viewModelInstance =
                            m_file->createDefaultViewModelInstance(viewModel);
                    }
                    // Store the default instance on the list item so we
                    // don't recreate one every time.
                    if (viewModelInstance != nullptr)
                    {
                        listItem->viewModelInstance(viewModelInstance);
                    }
                }
            }
        }

        // Fall back to the list item's VM instance if not stateful.
        if (viewModelInstance == nullptr)
        {
            viewModelInstance = listItem->viewModelInstance();
        }

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
    if (!virtualizationEnabled() && index >= 0 &&
        index < m_artboardInstancesByIndex.size())
    {
        m_artboardInstancesByIndex[index] = nullptr;
        m_stateMachinesByIndex[index] = nullptr;
    }
    auto item = listItem(index);
    removeArtboard(item);
}

void ArtboardComponentList::removeArtboard(rcp<ViewModelInstanceListItem> item)
{
    invalidateOrderedListIndicesCache();
    removeDrawIndexListenerForItem(item);
    auto itr = m_artboardInstancesMap.find(item);
    if (itr != m_artboardInstancesMap.end())
    {
        // Clean up focus tree before destroying the artboard to prevent
        // use-after-free when the FocusManager still holds references
        // to FocusNodes pointing to FocusData in this artboard.
        if (itr->second != nullptr)
        {
            itr->second->cleanupFocusTree();
        }
        clearArtboardOverride(itr->second.get());
    }
    // Remove bridge data binds before destroying the artboard.
    removeBridgeBinds(item);
    // Destroy state machines BEFORE artboards to ensure FocusListenerGroup
    // can unregister from FocusData before the artboard (and its FocusData)
    // is destroyed. Otherwise we get use-after-free in ~FocusListenerGroup.
    m_stateMachinesMap.erase(item);
    m_artboardInstancesMap.erase(item);
}

/// Returns the propertyValuePropertyKey for a ViewModelInstanceValue based
/// on its core type, or Core::invalidPropertyKey if unsupported.
static uint16_t propertyValueKeyForType(uint16_t coreType)
{
    switch (coreType)
    {
        case ViewModelInstanceNumberBase::typeKey:
            return ViewModelInstanceNumberBase::propertyValuePropertyKey;
        case ViewModelInstanceStringBase::typeKey:
            return ViewModelInstanceStringBase::propertyValuePropertyKey;
        case ViewModelInstanceColorBase::typeKey:
            return ViewModelInstanceColorBase::propertyValuePropertyKey;
        case ViewModelInstanceBooleanBase::typeKey:
            return ViewModelInstanceBooleanBase::propertyValuePropertyKey;
        case ViewModelInstanceEnumBase::typeKey:
            return ViewModelInstanceEnumBase::propertyValuePropertyKey;
        case ViewModelInstanceTriggerBase::typeKey:
            return ViewModelInstanceTriggerBase::propertyValuePropertyKey;
        case ViewModelInstanceViewModelBase::typeKey:
            return ViewModelInstanceViewModelBase::propertyValuePropertyKey;
        default:
            return Core::invalidPropertyKey;
    }
}

void ArtboardComponentList::createBridgeBinds(
    rcp<ViewModelInstanceListItem> listItem,
    ViewModelInstance* original,
    ViewModelInstance* clone)
{
    if (original == nullptr || clone == nullptr)
    {
        return;
    }
    auto* vm = clone->viewModel();
    if (vm == nullptr)
    {
        return;
    }
    auto* parentArtboard = artboard();
    if (parentArtboard == nullptr)
    {
        return;
    }

    auto& binds = m_bridgeDataBinds[listItem];

    for (auto& cloneValueRcp : clone->propertyValues())
    {
        auto* cloneValue = cloneValueRcp.get();
        auto* prop = cloneValue->viewModelProperty();
        if (prop == nullptr || (!prop->isInput() && !prop->isOutput()))
        {
            continue;
        }

        // Find the matching property on the original by ViewModelProperty
        // pointer (both instances share the same ViewModel definition).
        ViewModelInstanceValue* originalValue = nullptr;
        for (auto& origRcp : original->propertyValues())
        {
            if (origRcp->viewModelProperty() == prop)
            {
                originalValue = origRcp.get();
                break;
            }
        }
        if (originalValue == nullptr)
        {
            continue;
        }

        auto propKey = propertyValueKeyForType(cloneValue->coreType());
        if (propKey == Core::invalidPropertyKey)
        {
            continue;
        }

        if (prop->isInput())
        {
            // Input: original → clone (source to target)
            auto bind = std::make_unique<DataBind>();
            bind->source(ref_rcp(originalValue));
            bind->target(cloneValue);
            bind->propertyKey(propKey);
            bind->flags(static_cast<uint32_t>(DataBindFlags::ToTarget));
            bind->bind();
            parentArtboard->addDataBind(bind.get());
            binds.push_back(std::move(bind));
        }

        if (prop->isOutput())
        {
            // Output: clone → original. Uses ToSource direction so the
            // bind is in the persisting list and continuously syncs
            // changes made by the component's state machine back to
            // the user-provided VM instance.
            // ToSource semantics: reads from target, writes to source.
            auto bind = std::make_unique<DataBind>();
            bind->source(ref_rcp(originalValue));
            bind->target(cloneValue);
            bind->propertyKey(propKey);
            bind->flags(static_cast<uint32_t>(DataBindFlags::ToSource));
            bind->bind();
            parentArtboard->addDataBind(bind.get());
            binds.push_back(std::move(bind));
        }
    }
}

void ArtboardComponentList::removeBridgeBinds(
    const rcp<ViewModelInstanceListItem>& listItem)
{
    auto itr = m_bridgeDataBinds.find(listItem);
    if (itr == m_bridgeDataBinds.end())
    {
        return;
    }
    auto* parentArtboard = artboard();
    for (auto& bind : itr->second)
    {
        bind->unbind();
        if (parentArtboard != nullptr)
        {
            parentArtboard->removeDataBind(bind.get());
        }
    }
    m_bridgeDataBinds.erase(itr);
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
            auto sourceArtboard = nestedArtboard->sourceArtboard();
            createArtboardRecorders(sourceArtboard);
        }
    }
}

void ArtboardComponentList::applyRecorders(Artboard* artboard,
                                           const Artboard* sourceArtboard)
{
    auto sourcedArtboardIt = m_propertyRecordersMap.find(sourceArtboard);
    if (sourcedArtboardIt != m_propertyRecordersMap.end())
    {
        auto propertyRecorder = sourcedArtboardIt->second.get();
        propertyRecorder->apply(artboard);
    }
    for (auto& nestedArtboard : artboard->nestedArtboards())
    {
        applyRecorders(nestedArtboard->sourceArtboard(),
                       nestedArtboard->sourceArtboard()->artboardSource());
    }
}

void ArtboardComponentList::applyRecorders(
    StateMachineInstance* stateMachineInstance,
    const Artboard* sourceArtboard)
{
    auto propertyRecorder = m_propertyRecordersMap[sourceArtboard].get();
    propertyRecorder->apply(stateMachineInstance);
}

void ArtboardComponentList::addVirtualizable(int index)
{
    auto listItem = this->listItem(index);
    if (listItem != nullptr)
    {
        auto artboard = findArtboard(listItem);
        if (artboard != nullptr)
        {
            createArtboardRecorders(artboard);
            auto& pool = m_resourcePool[artboard];
            if (pool.empty())
            {
                createArtboardAt(index);
            }
            else
            {
                auto pooledArtboard = pool.back().get();
                applyRecorders(pooledArtboard, artboard);
                addArtboardAt(std::move(pool.back()), index);
                pool.pop_back();
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

void ArtboardComponentList::removeVirtualizable(int index)
{
    auto listItem = this->listItem(index);
    if (listItem != nullptr)
    {
        auto artboard = findArtboard(listItem);
        auto artboardInstance = std::move(m_artboardInstancesMap[listItem]);
        if (artboard != nullptr && artboardInstance != nullptr)
        {
            auto& pool = m_resourcePool[artboard];
            pool.push_back(std::move(artboardInstance));
        }
        auto smInstanceIterator = m_stateMachinesMap.find(listItem);
        if (smInstanceIterator != m_stateMachinesMap.end())
        {
            auto& smPool = m_stateMachinesPool[artboard];
            smPool.push_back(std::move(smInstanceIterator->second));
        }
    }
    removeArtboardAt(index);
}

void ArtboardComponentList::setVirtualizablePosition(int index, Vec2D position)
{
    auto artboard = this->artboardInstance(index);
    if (artboard != nullptr)
    {
        auto useLayout = layoutParent() != nullptr;
        auto origin = useLayout ? artboard->origin() : Vec2D();
        m_artboardTransforms[artboard] =
            Mat2D::fromTranslate(position.x - origin.x, position.y - origin.y);
    }
}

bool ArtboardComponentList::virtualizationEnabled()
{
    auto virtualizer = scrollConstraint();
    return virtualizer != nullptr && virtualizer->virtualize();
}

ScrollConstraint* ArtboardComponentList::scrollConstraint()
{
    for (auto parentConstraint : m_layoutConstraints)
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
        auto runningWidth = 0.0f;
        auto runningHeight = 0.0f;
        bool isHorz = mainAxisIsRow();
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
    if (index < m_artboardSizes.size())
    {
        m_artboardSizes[index] = size;
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

void ArtboardComponentList::attachArtboardOverride(
    ArtboardInstance* instance,
    rcp<ViewModelInstanceListItem> listItem)
{

    auto viewModelInstance = listItem->viewModelInstance();
    if (viewModelInstance == nullptr)
    {
        return;
    }
    auto artboards = m_file->artboards();
    int artboardIndex = -1;
    for (auto& artboard : artboards)
    {
        artboardIndex++;

        if (artboard->viewModelId() == viewModelInstance->viewModelId())
        {
            break;
        }
    }
    if (artboardIndex < 0 && artboardIndex >= artboards.size())
    {
        return;
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
            else if (child->as<ArtboardComponentListOverride>()->artboardId() ==
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

LayoutComponent* ArtboardComponentList::layoutParent()
{
    if (parent() != nullptr && parent()->is<LayoutComponent>())
    {
        return parent()->as<LayoutComponent>();
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