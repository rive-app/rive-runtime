#include "rive/scripted/scripted_transition.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/component.hpp"
#include "rive/component_dirt.hpp"
#include "rive/drawable_flag.hpp"
#include "rive/enums.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "rive/layout_component.hpp"
#endif

using namespace rive;

// v1: only NestedArtboard children participate in the design-time solo set.
static bool isTransitionChild(Component* child)
{
    return child->is<NestedArtboard>();
}

// Toggle the loop-exclusion (Hidden drawable flag) on a design child. Hidden
// removes the child from the artboard draw walk (Drawable::isHidden()) without
// collapsing it, so NestedArtboard::advanceComponent keeps advancing its
// content while the container composites it in draw().
static void setChildHidden(Drawable* drawable, bool hidden)
{
    uint32_t flags = drawable->drawableFlags();
    if (hidden)
    {
        flags |= static_cast<uint32_t>(DrawableFlag::Hidden);
    }
    else
    {
        flags &= ~static_cast<uint32_t>(DrawableFlag::Hidden);
    }
    drawable->drawableFlags(flags);
}

ScriptedTransition::~ScriptedTransition() { clearListInstances(); }

// ---------------------------------------------------------------------------
// Design-time solo set
// ---------------------------------------------------------------------------

int ScriptedTransition::designChildCount()
{
    int count = 0;
    for (Component* child : children())
    {
        if (isTransitionChild(child))
        {
            count++;
        }
    }
    return count;
}

NestedArtboard* ScriptedTransition::designChildAt(int index)
{
    int i = 0;
    for (Component* child : children())
    {
        if (!isTransitionChild(child))
        {
            continue;
        }
        if (i == index)
        {
            return child->as<NestedArtboard>();
        }
        i++;
    }
    return nullptr;
}

int ScriptedTransition::sourceIndex(const TransitionSource& source)
{
    if (source.instance != nullptr)
    {
        for (size_t i = 0; i < m_listInstancesByIndex.size(); i++)
        {
            if (m_listInstancesByIndex[i] == source.instance)
            {
                return designChildCount() + (int)i;
            }
        }
        return -1;
    }
    if (source.design != nullptr)
    {
        int i = 0;
        for (Component* child : children())
        {
            if (!isTransitionChild(child))
            {
                continue;
            }
            if (child == source.design)
            {
                return i;
            }
            i++;
        }
    }
    return -1;
}

Component* ScriptedTransition::activeComponent()
{
    auto* ab = artboard();
    Core* active = ab != nullptr ? ab->resolve(activeComponentId()) : nullptr;
    for (Component* child : children())
    {
        if (child == active && isTransitionChild(child))
        {
            return child;
        }
    }
    return nullptr;
}

bool ScriptedTransition::scriptReady() const
{
#ifdef WITH_RIVE_SCRIPTING
    return m_vm != nullptr && m_self != 0;
#else
    return false;
#endif
}

bool ScriptedTransition::scriptManagesTo() const
{
#ifdef WITH_RIVE_SCRIPTING
    if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
    {
        return true;
    }
    return m_vm->transitionManagesTo(m_self);
#else
    return true;
#endif
}

void ScriptedTransition::propagateCollapse(bool collapse)
{
    Core* active = collapse || artboard() == nullptr
                       ? nullptr
                       : artboard()->resolve(activeComponentId());
    for (Component* child : children())
    {
        if (!isTransitionChild(child))
        {
            child->collapse(collapse);
            continue;
        }
        if (!collapse && m_transitioning &&
            (child == m_from.design || child == m_to.design))
        {
            // Both transitioning design children stay live; drawn by draw().
            child->collapse(false);
        }
        else
        {
            child->collapse(child != active);
        }
    }
}

bool ScriptedTransition::collapse(bool value)
{
    if (!Component::collapse(value))
    {
        return false;
    }
    propagateCollapse(value);
    return true;
}

// ---------------------------------------------------------------------------
// List instancing (lazy) — modeled on ArtboardComponentList
// ---------------------------------------------------------------------------

Artboard* ScriptedTransition::findListArtboard(
    const rcp<ViewModelInstanceListItem>& item)
{
    if (m_file == nullptr || item == nullptr)
    {
        return nullptr;
    }
    auto vmi = item->viewModelInstance();
    if (vmi == nullptr)
    {
        return nullptr;
    }
    uint32_t viewModelId = vmi->viewModelId();
    auto cached = m_artboardsMap.find(viewModelId);
    if (cached != m_artboardsMap.end())
    {
        return cached->second;
    }
    for (auto ab : m_file->artboards())
    {
        if (ab->viewModelId() == viewModelId)
        {
            m_artboardsMap[viewModelId] = ab;
            return ab;
        }
    }
    return nullptr;
}

ArtboardInstance* ScriptedTransition::ensureListInstance(int index)
{
    if (index < 0 || index >= (int)m_listItems.size())
    {
        return nullptr;
    }
    if (index < (int)m_listInstancesByIndex.size() &&
        m_listInstancesByIndex[index] != nullptr)
    {
        return m_listInstancesByIndex[index];
    }
    auto& item = m_listItems[index];
    auto existing = m_listInstances.find(item);
    if (existing != m_listInstances.end())
    {
        return existing->second.get();
    }
    Artboard* source = findListArtboard(item);
    if (source == nullptr)
    {
        return nullptr;
    }
    std::unique_ptr<ArtboardInstance> inst = source->instance();
    ArtboardInstance* raw = inst.get();
    if (raw == nullptr)
    {
        return nullptr;
    }
    if (artboard() != nullptr)
    {
        raw->bindViewModelInstance(item->viewModelInstance(),
                                   artboard()->dataContext());
        raw->updateDataBinds();
    }
    raw->frameOrigin(false);
    // Default state machine so the instanced artboard animates.
    int smIndex = raw->defaultStateMachineIndex();
    std::unique_ptr<StateMachineInstance> sm =
        raw->stateMachineAt(smIndex >= 0 ? (size_t)smIndex : 0u);
    if (sm != nullptr)
    {
        sm->dataContext(raw->dataContext());
        sm->updateDataBinds(false);
        m_listStateMachines[raw] = std::move(sm);
    }
    m_listInstances[item] = std::move(inst);
    if ((int)m_listInstancesByIndex.size() <= index)
    {
        m_listInstancesByIndex.resize(m_listItems.size(), nullptr);
    }
    m_listInstancesByIndex[index] = raw;
    return raw;
}

void ScriptedTransition::forgetInstance(ArtboardInstance* instance)
{
    if (instance == nullptr)
    {
        return;
    }
    if (m_transitioning &&
        (m_from.instance == instance || m_to.instance == instance))
    {
        // The transition can't run against a source that's about to be freed:
        // settle it now, which also un-hides any design child it had hidden.
        completeTransition();
    }
    if (m_current.instance == instance)
    {
        // completeTransition() may have just seated it as current.
        m_current = TransitionSource();
    }
}

void ScriptedTransition::disposeListItem(
    const rcp<ViewModelInstanceListItem>& item)
{
    auto instIt = m_listInstances.find(item);
    if (instIt != m_listInstances.end())
    {
        forgetInstance(instIt->second.get());
        // State machine before artboard (avoids use-after-free in teardown).
        m_listStateMachines.erase(instIt->second.get());
        m_listInstances.erase(instIt);
    }
}

void ScriptedTransition::clearListInstances()
{
    if (m_transitioning &&
        (m_from.instance != nullptr || m_to.instance != nullptr))
    {
        completeTransition();
    }
    if (m_current.instance != nullptr)
    {
        m_current = TransitionSource();
    }
    m_listStateMachines.clear(); // state machines before artboards
    m_listInstances.clear();
    m_listInstancesByIndex.clear();
    m_listItems.clear();
    m_artboardsMap.clear();
    m_activeListIndex = -1;
}

static bool listsAreEqual(
    const std::vector<rcp<ViewModelInstanceListItem>>& current,
    std::vector<rcp<ViewModelInstanceListItem>>* incoming)
{
    if (incoming == nullptr)
    {
        return current.empty();
    }
    if (current.size() != incoming->size())
    {
        return false;
    }
    for (size_t i = 0; i < current.size(); i++)
    {
        if (current[i] != (*incoming)[i])
        {
            return false;
        }
    }
    return true;
}

void ScriptedTransition::updateList(
    std::vector<rcp<ViewModelInstanceListItem>>* list)
{
    if (listsAreEqual(m_listItems, list))
    {
        return;
    }
    std::vector<rcp<ViewModelInstanceListItem>> old = m_listItems;
    if (list != nullptr)
    {
        m_listItems = *list;
    }
    else
    {
        m_listItems.clear();
    }
    // Dispose cached instances for items no longer present.
    for (auto& item : old)
    {
        bool present = false;
        for (auto& current : m_listItems)
        {
            if (current == item)
            {
                present = true;
                break;
            }
        }
        if (!present)
        {
            disposeListItem(item);
        }
    }
    // Assign each item its `index` symbol (if any) + rebuild the lazy index
    // cache, keeping instances that survived.
    m_listInstancesByIndex.assign(m_listItems.size(), nullptr);
    for (size_t i = 0; i < m_listItems.size(); i++)
    {
        m_listItems[i]->assignListIndex((uint32_t)i);
        auto it = m_listInstances.find(m_listItems[i]);
        if (it != m_listInstances.end())
        {
            m_listInstancesByIndex[i] = it->second.get();
        }
    }
    addDirt(ComponentDirt::Components);
    applySelection(); // clamps m_activeListIndex and (re)selects
    wakeAdvance();
}

void ScriptedTransition::listSourceChanged()
{
    // Unbound: clear the instanced children.
    if (listSource() == (uint32_t)-1)
    {
        std::vector<rcp<ViewModelInstanceListItem>> empty;
        updateList(&empty);
    }
}

// ---------------------------------------------------------------------------
// Unified selection over the combined solo set
// ---------------------------------------------------------------------------

ScriptedTransition::TransitionSource ScriptedTransition::intendedSource()
{
    TransitionSource source;
    if (m_activeListIndex >= 0 && m_activeListIndex < (int)m_listItems.size())
    {
        source.instance = ensureListInstance(m_activeListIndex);
        if (source.instance != nullptr)
        {
            return source;
        }
    }
    Component* design = activeComponent();
    if (design != nullptr && design->is<NestedArtboard>())
    {
        source.design = design->as<NestedArtboard>();
    }
    return source;
}

void ScriptedTransition::applySelection()
{
    selectActiveSource(intendedSource());
}

void ScriptedTransition::selectActiveSource(const TransitionSource& next)
{
    if (next == m_current)
    {
        return;
    }
    TransitionSource prev = m_current;
    m_current = next;
    if (scriptReady() && !prev.empty() && !next.empty())
    {
        startTransition(prev, next);
        return;
    }
    // Instant: clear any in-flight transition and settle collapse.
    if (m_transitioning)
    {
        if (m_from.design != nullptr)
        {
            setChildHidden(m_from.design, false);
        }
        if (m_to.design != nullptr)
        {
            setChildHidden(m_to.design, false);
        }
        m_transitioning = false;
        m_from = TransitionSource();
        m_to = TransitionSource();
        m_manageTo = true;
    }
    propagateCollapse(isCollapsed());
    addScriptedDirt(ComponentDirt::Paint);
}

void ScriptedTransition::startTransition(const TransitionSource& from,
                                         const TransitionSource& to)
{
    TransitionSource previousFrom =
        m_transitioning ? m_from : TransitionSource();

    m_from = from;
    m_to = to;
    m_transitioning = true;

    // Interruption: retire a previously-outgoing design child not in the new
    // pair (list instances need no un-hide — they are never in the draw loop).
    if (!previousFrom.empty() && previousFrom != m_from &&
        previousFrom != m_to && previousFrom.design != nullptr)
    {
        setChildHidden(previousFrom.design, false);
        previousFrom.design->collapse(true);
    }

    // Keep design sources live; list instances are host-drawn regardless.
    if (m_from.design != nullptr)
    {
        m_from.design->collapse(false);
    }
    if (m_to.design != nullptr)
    {
        m_to.design->collapse(false);
    }
    propagateCollapse(isCollapsed());

    callChanged(m_from, m_to);

    // A list `to` has no draw-loop entry to fall back to, so it must always be
    // script-composited; a design `to` may opt out via `managesTo`.
    m_manageTo = m_to.instance != nullptr ? true : scriptManagesTo();
    if (m_from.design != nullptr)
    {
        setChildHidden(m_from.design, true);
    }
    if (m_to.design != nullptr)
    {
        setChildHidden(m_to.design, m_manageTo);
    }

    wakeAdvance();
    addScriptedDirt(ComponentDirt::Components);
}

void ScriptedTransition::completeTransition()
{
    if (!m_transitioning)
    {
        return;
    }
    TransitionSource from = m_from;
    TransitionSource to = m_to;
    m_transitioning = false;
    m_from = TransitionSource();
    m_to = TransitionSource();
    m_manageTo = true;
    if (from.design != nullptr)
    {
        setChildHidden(from.design, false);
    }
    if (to.design != nullptr)
    {
        setChildHidden(to.design, false);
    }
    m_current = to;
    propagateCollapse(isCollapsed());
    addScriptedDirt(ComponentDirt::Paint);
}

void ScriptedTransition::activeComponentIdChanged()
{
    // A genuine design child was chosen -> leave list mode.
    if (activeComponent() != nullptr)
    {
        m_activeListIndex = -1;
    }
    applySelection();
#ifdef WITH_RIVE_LAYOUT
    recollectOwningLayout();
#endif
}

StatusCode ScriptedTransition::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    m_current = intendedSource();
    propagateCollapse(isCollapsed());
#ifdef WITH_RIVE_LAYOUT
    recollectOwningLayout();
#endif
    return StatusCode::Ok;
}

// ---------------------------------------------------------------------------
// Advance / draw / update
// ---------------------------------------------------------------------------

void ScriptedTransition::advanceLiveInstance(const TransitionSource& source,
                                             float elapsedSeconds,
                                             AdvanceFlags flags,
                                             bool& keepGoing)
{
    if (source.instance == nullptr)
    {
        return;
    }
    auto smIt = m_listStateMachines.find(source.instance);
    StateMachineInstance* sm =
        smIt != m_listStateMachines.end() ? smIt->second.get() : nullptr;
    if (sm != nullptr && enums::is_flag_set(flags, AdvanceFlags::AdvanceNested))
    {
        if (sm->advanceAndApply(elapsedSeconds, false))
        {
            keepGoing = true;
        }
    }
    else if (source.instance->advanceInternal(elapsedSeconds, flags))
    {
        keepGoing = true;
    }
}

bool ScriptedTransition::advanceComponent(float elapsedSeconds,
                                          AdvanceFlags flags)
{
    bool keepGoing = false;

    if (m_transitioning)
    {
        // In design mode (no nested/real time), settle immediately rather than
        // freezing mid-blend.
        if (!enums::is_flag_set(flags, AdvanceFlags::AdvanceNested))
        {
            completeTransition();
        }
        else if (elapsedSeconds == 0)
        {
            // Waiting for real elapsed time; keep requesting frames.
            keepGoing = true;
        }
        else
        {
            bool scriptGoing = scriptAdvance(elapsedSeconds);
            addScriptedDirt(ComponentDirt::Paint);
            if (!scriptGoing)
            {
                completeTransition();
            }
            else
            {
                keepGoing = true;
            }
        }
    }

    // Advance the live list instance(s). Design sources advance via the
    // artboard's own advancing-component loop, so only list instances here.
    if (m_transitioning)
    {
        advanceLiveInstance(m_from, elapsedSeconds, flags, keepGoing);
        advanceLiveInstance(m_to, elapsedSeconds, flags, keepGoing);
    }
    else
    {
        advanceLiveInstance(m_current, elapsedSeconds, flags, keepGoing);
    }
    return keepGoing;
}

void ScriptedTransition::draw(Renderer* renderer)
{
#ifdef WITH_RIVE_SCRIPTING
    if (m_transitioning && m_vm != nullptr && m_vm->valid() && draws())
    {
        ScriptBackend::TransitionChildRef from = childRef(m_from);
        ScriptBackend::TransitionChildRef to = childRef(m_to);
        // When the script opts out of managing the to child (design only) it
        // renders through the normal loop, so hand the script nil instead.
        if (!m_manageTo)
        {
            to.artboard = nullptr;
        }

        renderer->save();
        m_vm->callTransitionDraw(this, m_self, renderer, from, to);
        renderer->restore();
        return;
    }
#endif
    // Idle: an active LIST instance has no draw-loop entry, so draw it here
    // (design children draw through the normal loop, gated by collapse).
    if (m_current.instance != nullptr)
    {
        renderer->save();
        renderer->transform(worldTransform());
        artboard()->drawHosted(m_current.instance, renderer);
        renderer->restore();
    }
}

bool ScriptedTransition::willDraw()
{
    return Super::willDraw() &&
           (m_transitioning || m_current.instance != nullptr);
}

void ScriptedTransition::update(ComponentDirt value)
{
    Super::update(value);

    auto forward = [this, value](const TransitionSource& source) {
        ArtboardInstance* inst = source.instance;
        if (inst == nullptr)
        {
            return;
        }
        if ((value & ComponentDirt::RenderOpacity) ==
            ComponentDirt::RenderOpacity)
        {
            inst->opacity(renderOpacity());
        }
        if ((value & ComponentDirt::Components) == ComponentDirt::Components)
        {
            inst->updatePass(false);
        }
    };
    if (m_transitioning)
    {
        forward(m_from);
        forward(m_to);
    }
    else
    {
        forward(m_current);
    }
}

void ScriptedTransition::callChanged(const TransitionSource& from,
                                     const TransitionSource& to)
{
#ifdef WITH_RIVE_SCRIPTING
    if (m_vm == nullptr || !m_vm->valid())
    {
        return;
    }
    // 1 when the incoming child is at a higher combined index than the
    // outgoing one, -1 when lower, 0 when unknown (e.g. first show).
    int fromIndex = sourceIndex(from);
    int toIndex = sourceIndex(to);
    int direction =
        (fromIndex >= 0 && toIndex >= 0)
            ? (toIndex > fromIndex ? 1 : (toIndex < fromIndex ? -1 : 0))
            : 0;
    m_vm->callTransitionChanged(this,
                                m_self,
                                childRef(from),
                                childRef(to),
                                direction);
#endif
}

#ifdef WITH_RIVE_SCRIPTING
ScriptBackend::TransitionChildRef ScriptedTransition::childRef(
    const TransitionSource& source) const
{
    ScriptBackend::TransitionChildRef ref;
    if (source.design != nullptr)
    {
        ref.artboard = static_cast<Artboard*>(source.design->sourceArtboard());
        ref.transform = source.design->worldTransform();
    }
    else
    {
        ref.artboard = static_cast<Artboard*>(source.instance);
        ref.transform = worldTransform();
    }
    return ref;
}
#endif

Core* ScriptedTransition::clone() const
{
    ScriptedTransition* twin =
        ScriptedTransitionBase::clone()->as<ScriptedTransition>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    twin->file(m_file);
    return twin;
}

#ifdef WITH_RIVE_LAYOUT
void ScriptedTransition::recollectOwningLayout()
{
    for (Component* p = parent(); p != nullptr; p = p->parent())
    {
        if (p->is<LayoutComponent>())
        {
            p->as<LayoutComponent>()->syncLayoutChildren();
            return;
        }
    }
}
#endif

// ---------------------------------------------------------------------------
// Combined-index selectors (design children then list instances)
// ---------------------------------------------------------------------------

void ScriptedTransition::updateByIndex(size_t index)
{
    int designCount = designChildCount();
    // The data-binding path casts rounded floats to size_t without clamping,
    // so a negative binding arrives as a huge index. Reject it before the
    // narrowing below turns it back into a negative int, which would pass the
    // design-child test and clear an active list selection (mirrors
    // Solo::updateByIndex).
    if (index >= (size_t)designCount + m_listItems.size())
    {
        return;
    }
    if ((int)index < designCount)
    {
        m_activeListIndex = -1;
        NestedArtboard* child = designChildAt((int)index);
        if (child != nullptr && artboard() != nullptr)
        {
            activeComponentId(artboard()->idOf(child));
        }
        applySelection();
        return;
    }
    size_t listIndex = index - (size_t)designCount;
    if (listIndex >= m_listItems.size())
    {
        return;
    }
    m_activeListIndex = (int)listIndex;
    // Neutralize the design selection (0 == runtime "missing"); resolves to no
    // child, so activeComponentIdChanged leaves m_activeListIndex intact.
    if (activeComponentId() != 0)
    {
        activeComponentId(0);
    }
    applySelection();
}

void ScriptedTransition::updateByName(const std::string& name)
{
    // v1: name selection covers design children only (list items have no
    // stable names).
    if (!artboard())
    {
        return;
    }
    for (auto& child : children())
    {
        if (!isTransitionChild(child))
        {
            continue;
        }
        if (child->name() == name)
        {
            m_activeListIndex = -1;
            activeComponentId(artboard()->idOf(child));
            break;
        }
    }
}

int ScriptedTransition::getActiveChildIndex()
{
    if (m_activeListIndex >= 0)
    {
        return designChildCount() + m_activeListIndex;
    }
    if (!artboard())
    {
        return -1;
    }
    Core* active = artboard()->resolve(activeComponentId());
    if (active)
    {
        int index = 0;
        for (auto& child : children())
        {
            if (!isTransitionChild(child))
            {
                continue;
            }
            if (child == active)
            {
                return index;
            }
            index++;
        }
    }
    return -1;
}

std::string ScriptedTransition::getActiveChildName()
{
    if (!artboard())
    {
        return "";
    }
    Core* active = artboard()->resolve(activeComponentId());
    if (active && active->is<Component>())
    {
        return active->as<Component>()->name();
    }
    return "";
}
