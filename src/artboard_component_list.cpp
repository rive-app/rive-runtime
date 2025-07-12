#include "rive/component.hpp"
#include "rive/file.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout_component.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/layout/layout_data.hpp"

using namespace rive;

ArtboardComponentList::ArtboardComponentList() {}
ArtboardComponentList::~ArtboardComponentList() { reset(); }

void ArtboardComponentList::reset()
{
    for (auto& artboard : m_artboardInstancesMap)
    {
        artboard.second.reset();
    }
    for (auto& sm : m_stateMachinesMap)
    {
        sm.second.reset();
    }
    m_artboardInstancesMap.clear();
    m_stateMachinesMap.clear();
    m_listItems.clear();
    m_artboardsMap.clear();
    m_resourcePool.clear();
    m_stateMachinesPool.clear();
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
    if (index < m_listItems.size())
    {
        auto item = listItem(index);
        auto itr = m_artboardInstancesMap.find(item);
        if (itr != m_artboardInstancesMap.end())
        {
            return m_artboardInstancesMap[item].get();
        }
    }
    return nullptr;
}
StateMachineInstance* ArtboardComponentList::stateMachineInstance(int index)
{
    if (index < m_listItems.size())
    {
        auto item = listItem(index);
        auto itr = m_stateMachinesMap.find(item);
        if (itr != m_stateMachinesMap.end())
        {
            return m_stateMachinesMap[item].get();
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
    bool parentIsRow = true;
    if (parent()->is<LayoutComponent>())
    {
        parentIsRow = parent()->as<LayoutComponent>()->mainAxisIsRow();
    }
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
    auto artboard = m_artboardsMap.find(viewModelInstance->viewModelId());
    if (artboard != m_artboardsMap.end())
    {
        return artboard->second;
    }
    auto artboards = m_file->artboards();
    for (auto& artboard : artboards)
    {
        if (artboard->viewModelId() == viewModelInstance->viewModelId())
        {
            m_artboardsMap[viewModelInstance->viewModelId()] = artboard;
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
    ArtboardInstance* artboard)
{
    if (artboard != nullptr)
    {
        auto dataContext = artboard->dataContext();
        stateMachineInstance->dataContext(dataContext);
        // TODO: @hernan added this to make sure data binds are procesed in the
        // current frame instead of waiting for the next run. But might not be
        // necessary. Needs more testing.
        stateMachineInstance->updateDataBinds();
    }
}

void ArtboardComponentList::updateList(
    int propertyKey,
    std::vector<rcp<ViewModelInstanceListItem>>* list)
{
    m_oldItems.clear();
    m_oldItems.assign(m_listItems.begin(), m_listItems.end());
    m_listItems.clear();
    m_listItems.assign(list->begin(), list->end());
    m_artboardSizes.clear();

    if (parent()->is<LayoutComponent>())
    {
#ifdef WITH_RIVE_LAYOUT
        parent()->as<LayoutComponent>()->clearLayoutChildren();
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
        if (!virtualizationEnabled() && itr == m_artboardInstancesMap.end())
        {
            createArtboardAt(index);
        }
        index++;
    }
    computeLayoutBounds();
    syncLayoutChildren();
    markLayoutNodeDirty();
    addDirt(ComponentDirt::Components);
}

void ArtboardComponentList::syncLayoutChildren()
{
    if (parent()->is<LayoutComponent>())
    {
#ifdef WITH_RIVE_LAYOUT
        parent()->as<LayoutComponent>()->syncLayoutChildren();
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
        bool isHorizontal = true;
        if (parent()->is<LayoutComponent>())
        {
            isHorizontal = parent()->as<LayoutComponent>()->mainAxisIsRow();
        }
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
}

void ArtboardComponentList::draw(Renderer* renderer)
{
    if (m_listItems.size() == 0)
    {
        return;
    }
    ClipResult clipResult = applyClip(renderer);
    if (clipResult == ClipResult::noClip)
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }
    if (clipResult != ClipResult::emptyClip)
    {
        if (virtualizationEnabled())
        {
            renderer->transform(
                parent()->as<LayoutComponent>()->worldTransform());
            if (m_visibleStartIndex != -1 && m_visibleEndIndex != -1)
            {
                // We need to render in the correct order so we get the correct
                // z-index for items in cases where there is overlap
                auto startIndex = m_visibleStartIndex % (int)m_listItems.size();
                auto endIndex = m_visibleEndIndex % (int)m_listItems.size();
                int i = startIndex;
                while (true)
                {
                    auto artboard = artboardInstance(i);
                    if (artboard != nullptr)
                    {
                        renderer->save();
                        auto position = m_artboardPositions[artboard];
                        auto artboardTransform =
                            Mat2D::fromTranslate(position.x, position.y);
                        renderer->transform(artboardTransform);
                        artboard->draw(renderer);
                        renderer->restore();
                    }
                    if (i == endIndex)
                    {
                        break;
                    }
                    i = (i + 1) % m_listItems.size();
                }
            }
        }
        else
        {
            renderer->transform(worldTransform());
            for (int i = 0; i < artboardCount(); i++)
            {
                auto artboard = artboardInstance(i);
                if (artboard != nullptr)
                {
                    renderer->save();
                    auto bounds = artboard->layoutBounds();
                    auto artboardTransform =
                        Mat2D::fromTranslate(bounds.left(), bounds.top());
                    renderer->transform(artboardTransform);
                    artboard->draw(renderer);
                    renderer->restore();
                }
            }
        }
    }
    renderer->restore();
}

Core* ArtboardComponentList::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

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

void ArtboardComponentList::updateConstraints()
{
    if (m_layoutConstraints.size() > 0)
    {
        for (auto parentConstraint : m_layoutConstraints)
        {
            parentConstraint->constrainChild(this);
        }
    }
    Super::updateConstraints();
}

void ArtboardComponentList::internalDataContext(DataContext* value)
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
    DataContext* parent)
{
    // At the time this is called, artboards will not yet have been instanced
    // so its essentially a no-op and the call to bindViewModelInstance on
    // each artboard is made at the time of artboard instancing
}

void ArtboardComponentList::clearDataContext() {}
void ArtboardComponentList::unbind() { reset(); }
void ArtboardComponentList::updateDataBinds()
{
    for (int i = 0; i < artboardCount(); i++)
    {
        auto stateMachine = stateMachineInstance(i);
        if (stateMachine != nullptr)
        {
            stateMachine->updateDataBinds();
        }
        auto artboard = artboardInstance(i);
        if (artboard != nullptr)
        {
            artboard->updateDataBinds();
        }
    }
}

bool ArtboardComponentList::worldToLocal(Vec2D world, Vec2D* local, int index)
{
    assert(local != nullptr);
    auto artboard = artboardInstance(index);
    if (artboard == nullptr)
    {
        return false;
    }
    auto offset = virtualizationEnabled()
                      ? m_artboardPositions[artboard]
                      : Vec2D(artboard->layoutBounds().left(),
                              artboard->layoutBounds().top());
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

void ArtboardComponentList::createArtboardAt(int index)
{
    auto item = listItem(index);
    if (item != nullptr)
    {
        auto artboardCopy = createArtboard(this, item);
        if (artboardCopy != nullptr)
        {
            addArtboardAt(std::move(artboardCopy), index);
        }
    }
}

void ArtboardComponentList::addArtboardAt(
    std::unique_ptr<ArtboardInstance> artboard,
    int index)
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
        }
        syncLayoutChildren();
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
                smPool.pop_back();
                return;
            }
        }
        auto stateMachineCopy =
            createStateMachineInstance(this, artboardInstance);
        m_stateMachinesMap[item] = std::move(stateMachineCopy);
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
        // TODO: @hernan added this to make sure data binds are procesed in the
        // current frame instead of waiting for the next run. But might not be
        // necessary. Needs more testing.
        artboardInstance->bindViewModelInstance(listItem->viewModelInstance(),
                                                dataContext);
        artboardInstance->updateDataBinds();
    }
}

void ArtboardComponentList::removeArtboardAt(int index)
{
    auto item = listItem(index);
    removeArtboard(item);
}

void ArtboardComponentList::removeArtboard(rcp<ViewModelInstanceListItem> item)
{
    m_artboardInstancesMap.erase(item);
    m_stateMachinesMap.erase(item);
}

void ArtboardComponentList::createArtboardRecorders(Artboard* artboard)
{
    if (artboard == nullptr)
    {
        return;
    }
    auto recorderIt = m_propertyRecordersMap.find(artboard);
    if (recorderIt == m_propertyRecordersMap.end())
    {
        auto propertyRecorder = rivestd::make_unique<PropertyRecorder>();
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
                                           Artboard* sourceArtboard)
{
    auto propertyRecorder = m_propertyRecordersMap[sourceArtboard].get();
    propertyRecorder->apply(artboard);
    auto artboards = m_file->artboards();
    for (auto& nestedArtboard : artboard->nestedArtboards())
    {
        if (nestedArtboard->artboardId() < artboards.size())
        {
            auto sourceNestedArtboard = artboards[nestedArtboard->artboardId()];
            applyRecorders(nestedArtboard->sourceArtboard(),
                           sourceNestedArtboard);
        }
    }
}

void ArtboardComponentList::applyRecorders(
    StateMachineInstance* stateMachineInstance,
    Artboard* sourceArtboard)
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
            if (parent()->is<LayoutComponent>())
            {
                parent()->as<LayoutComponent>()->markLayoutStyleDirty();
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
        m_artboardPositions[artboard] = position;
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
        bool isHorz = true;
        if (parent()->is<LayoutComponent>())
        {
            isHorz = parent()->as<LayoutComponent>()->mainAxisIsRow();
        }
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
    if (parent()->is<LayoutComponent>())
    {
        auto layoutParent = parent()->as<LayoutComponent>();
        return layoutParent->mainAxisIsRow() ? layoutParent->gapHorizontal()
                                             : layoutParent->gapVertical();
    }
    return 0.0f;
}