#include "rive/component.hpp"
#include "rive/file.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/layout_constraint.hpp"
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
    for (auto& item : m_listItems)
    {
        item->unref();
    }
    m_artboardInstancesMap.clear();
    m_stateMachinesMap.clear();
    m_listItems.clear();
    m_artboardsMap.clear();
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
        }
    }
#endif
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
    ViewModelInstanceListItem* listItem) const
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

void ArtboardComponentList::disposeListItem(ViewModelInstanceListItem* listItem)
{
    auto artboard = m_artboardInstancesMap[listItem].get();
    if (artboard != nullptr)
    {
        m_artboardInstancesMap[listItem].reset();
    }
    m_artboardInstancesMap.erase(listItem);
    auto sm = m_stateMachinesMap[listItem].get();
    if (sm != nullptr)
    {
        m_stateMachinesMap[listItem].reset();
    }
    m_stateMachinesMap.erase(listItem);
    listItem->unref();
}

std::unique_ptr<ArtboardInstance> ArtboardComponentList::createArtboard(
    Component* target,
    ViewModelInstanceListItem* listItem) const
{
    auto artboard = findArtboard(listItem);
    if (artboard != nullptr)
    {
        auto mainArtboard = target->artboard();
        auto dataContext = mainArtboard->dataContext();
        auto artboardCopy = artboard->instance();
        // artboardCopy->name(artboard->name());
        artboardCopy->bindViewModelInstance(listItem->viewModelInstance(),
                                            dataContext,
                                            true);
        return artboardCopy;
    }
    return nullptr;
}

std::unique_ptr<StateMachineInstance> ArtboardComponentList::
    createStateMachineInstance(Component* target, ArtboardInstance* artboard)
{
    if (artboard != nullptr)
    {
        auto dataContext = artboard->dataContext();
        auto stateMachineInstance = artboard->stateMachineAt(0);
        stateMachineInstance->dataContext(dataContext);
        return stateMachineInstance;
    }
    return nullptr;
}

void ArtboardComponentList::updateList(
    int propertyKey,
    std::vector<ViewModelInstanceListItem*>* list)
{
    std::vector<ViewModelInstanceListItem*> oldItems;
    oldItems.assign(m_listItems.begin(), m_listItems.end());
    m_listItems.clear();
    m_listItems.assign(list->begin(), list->end());
    for (auto item : oldItems)
    {
        auto it = std::find(m_listItems.begin(), m_listItems.end(), item);
        if (it == m_listItems.end())
        {
            disposeListItem(item);
        }
    }
    if (parent()->is<LayoutComponent>())
    {
#ifdef WITH_RIVE_LAYOUT
        parent()->as<LayoutComponent>()->clearLayoutChildren();
#endif
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
        if (m_artboardInstancesMap[item] == nullptr)
        {
            auto artboardCopy = createArtboard(this, item);
            if (artboardCopy != nullptr)
            {
                auto artboardInstance = artboardCopy.get();
                auto stateMachineCopy =
                    createStateMachineInstance(this, artboardInstance);
                m_artboardInstancesMap[item] = std::move(artboardCopy);
                m_stateMachinesMap[item] = std::move(stateMachineCopy);
                item->ref();
                if (artboardInstance != nullptr)
                {
                    artboardInstance->host(this);
                }
            }
        }
        index++;
    }
    if (parent()->is<LayoutComponent>())
    {
#ifdef WITH_RIVE_LAYOUT
        parent()->as<LayoutComponent>()->syncLayoutChildren();
#endif
    }
    markLayoutNodeDirty();
    addDirt(ComponentDirt::Components);
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

AABB ArtboardComponentList::layoutBounds() { return AABB(); }

AABB ArtboardComponentList::layoutBoundsForNode(int index)
{
    if (index >= 0 && index < numLayoutNodes())
    {
        return artboardInstance(index)->layoutBounds();
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
    ClipResult clipResult = applyClip(renderer);
    if (clipResult == ClipResult::noClip)
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }
    if (clipResult != ClipResult::emptyClip)
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
    // In the future, we may need to reconcile existing artboards with the
    // new datacontext passed in here
}

void ArtboardComponentList::populateDataBinds(std::vector<DataBind*>* dataBinds)
{
    // At the time this is called, artboards will not yet have been instanced
    // so its essentially a no-op and the data binds will be populated at
    // artboard creation time by calling artboard->bindViewModelInstance
    // passing isRoot=true
}

void ArtboardComponentList::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance,
    DataContext* parent)
{
    // At the time this is called, artboards will not yet have been instanced
    // so its essentially a no-op and the call to bindViewModelInstance on
    // each artboard is made at the time of artboard instancing
}

void ArtboardComponentList::clearDataContext() { reset(); }

bool ArtboardComponentList::worldToLocal(Vec2D world, Vec2D* local, int index)
{
    assert(local != nullptr);
    auto artboard = artboardInstance(index);
    if (artboard == nullptr)
    {
        return false;
    }
    auto bounds = artboard->layoutBounds();
    auto artboardTransform =
        worldTransform() * Mat2D::fromTranslate(bounds.left(), bounds.top());
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