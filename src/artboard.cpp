#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/backboard.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/custom_property_trigger.hpp"
#include "rive/dependency_sorter.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/draw_rules.hpp"
#include "rive/draw_target.hpp"
#include "rive/audio_event.hpp"
#include "rive/draw_target_placement.hpp"
#include "rive/drawable.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/layout_component.hpp"
#include "rive/foreground_layout_drawable.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/joystick.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/event.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/profiler/profiler_macros.h"
#include "rive/scripted/scripted_object.hpp"

#include <unordered_map>

using namespace rive;

Artboard::Artboard()
{
    // Artboards need to override default clip value to true.
    m_Clip = true;
#ifdef WITH_RIVE_TOOLS
    callbackUserData = this;
#endif
}

Artboard::~Artboard()
{
#ifdef WITH_RIVE_AUDIO
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    auto audioEngine = m_audioEngine;
#else
    auto audioEngine = AudioEngine::RuntimeEngine(false);
#endif
    if (audioEngine)
    {
        audioEngine->stop(this);
    }
#endif
    unbind();
    for (auto object : m_Objects)
    {
        // First object is artboard
        if (object == this)
        {
            continue;
        }
        delete object;
    }

    deleteDataBinds();

    // Instances reference back to the original artboard's animations and state
    // machines, so don't delete them here, they'll get cleaned up when the
    // source is deleted.
    // TODO: move this logic into ArtboardInstance destructor???
    if (!m_IsInstance)
    {
        for (auto object : m_Animations)
        {
            delete object;
        }
        for (auto object : m_StateMachines)
        {
            delete object;
        }
    }
    m_dirtyLayout.clear();
}

static bool canContinue(StatusCode code)
{
    // We currently only cease loading on invalid object.
    return code != StatusCode::InvalidObject;
}

bool Artboard::validateObjects()
{
    auto size = m_Objects.size();
    std::vector<bool> valid(size);

    // Max iterations..
    for (int cycle = 0; cycle < 100; cycle++)
    {
        bool changed = false;
        for (size_t i = 1; i < size; i++)
        {
            auto object = m_Objects[i];
            if (object == nullptr)
            {
                // objects can be null if they were not understood by this
                // runtime.
                continue;
            }
            bool wasValid = valid[i];
            bool isValid = object->validate(this);
            if (wasValid != isValid)
            {
                changed = true;
                valid[i] = isValid;
            }
        }
        if (changed)
        {
            // Delete invalid objects.
            for (size_t i = 1; i < size; i++)
            {
                if (valid[i])
                {
                    continue;
                }
                delete m_Objects[i];
                m_Objects[i] = nullptr;
            }
        }
        else
        {
            break;
        }
    }

    return true;
}

StatusCode Artboard::initialize()
{
    StatusCode code;

    // these will be re-built in update() -- are they needed here?
    m_layout = Layout(0.0f, 0.0f, width(), height());

#ifdef WITH_RIVE_LAYOUT
    markLayoutDirty(this);
#endif
    // onAddedDirty guarantees that all objects are now available so they can be
    // looked up by index/id. This is where nodes find their parents, but they
    // can't assume that their parent's parent will have resolved yet.
    for (auto object : m_Objects)
    {
        if (object == nullptr)
        {
            // objects can be null if they were not understood by this runtime.
            continue;
        }
        if (!canContinue(code = object->onAddedDirty(this)))
        {
            return code;
        }
    }

    // Animations and StateMachines initialize only once on the source/origin
    // Artboard. Instances will hold references to the original Animations and
    // StateMachines, so running this code for instances will effectively
    // initialize them twice. This can lead to unpredictable behaviour. One such
    // example was that resolved objects like listener inputs were being added
    // to lists twice.
    if (!isInstance())
    {
        for (auto object : m_Animations)
        {
            if (!canContinue(code = object->onAddedDirty(this)))
            {
                return code;
            }
        }

        for (auto object : m_StateMachines)
        {
            if (!canContinue(code = object->onAddedDirty(this)))
            {
                return code;
            }
        }
        if (m_Animations.size() == 0 && m_StateMachines.size() == 0)
        {
            auto sm = new StateMachine();
            sm->name("Auto Generated State Machine");
            m_StateMachines.push_back(sm);
        }
    }

    // Store a map of the drawRules to make it easier to lookup the matching
    // rule for a transform component.
    std::unordered_map<Core*, DrawRules*> componentDrawRules;

    // onAddedClean is called when all individually referenced components have
    // been found and so components can look at other components' references and
    // assume that they have resolved too. This is where the whole hierarchy is
    // linked up and we can traverse it to find other references (my parent's
    // parent should be type X can be checked now).
    for (auto object : m_Objects)
    {
        if (object == nullptr)
        {
            continue;
        }
        if (!canContinue(code = object->onAddedClean(this)))
        {
            return code;
        }
        if (object->is<Component>())
        {
            auto resettable = ResettingComponent::from(object->as<Component>());
            if (resettable)
            {
                m_Resettables.push_back(resettable);
            }
        }
        switch (object->coreType())
        {
            case DrawRulesBase::typeKey:
            {
                DrawRules* rules = static_cast<DrawRules*>(object);
                Core* component = resolve(rules->parentId());
                if (component != nullptr)
                {
                    componentDrawRules[component] = rules;
                }
                else
                {
                    fprintf(stderr,
                            "Artboard::initialize - Draw rule targets missing "
                            "component width id %d\n",
                            rules->parentId());
                }
                break;
            }
            case NestedArtboardBase::typeKey:
            case NestedArtboardLeafBase::typeKey:
            case NestedArtboardLayoutBase::typeKey:
            {
                m_NestedArtboards.push_back(object->as<NestedArtboard>());
                m_ArtboardHosts.push_back(object->as<NestedArtboard>());
                break;
            }
            case ArtboardComponentListBase::typeKey:
                m_ComponentLists.push_back(object->as<ArtboardComponentList>());
                m_ArtboardHosts.push_back(object->as<ArtboardComponentList>());
                break;
            case JoystickBase::typeKey:
            {
                Joystick* joystick = object->as<Joystick>();
                if (!joystick->canApplyBeforeUpdate())
                {
                    m_JoysticksApplyBeforeUpdate = false;
                }
                joystick->addDependents(this);
                m_Joysticks.push_back(joystick);
                break;
            }
        }
    }

    if (!isInstance())
    {
        for (auto object : m_Animations)
        {
            if (!canContinue(code = object->onAddedClean(this)))
            {
                return code;
            }
        }

        for (auto object : m_StateMachines)
        {
            if (!canContinue(code = object->onAddedClean(this)))
            {
                return code;
            }
        }
    }

    // Multi-level references have been built up, now we can
    // actually mark what's dependent on what.
    for (auto object : m_Objects)
    {
        if (object == nullptr)
        {
            continue;
        }
        if (object->is<Component>())
        {
            object->as<Component>()->buildDependencies();
        }
        if (object->is<Drawable>() && object != this)
        {
            Drawable* drawable = object->as<Drawable>();
            m_Drawables.push_back(drawable);
            // Move the foreground drawable before its parent. We traverse the
            // added list of drawables and swap their positions with the
            // foreground drawable until we find the parent
            if (drawable->is<ForegroundLayoutDrawable>())
            {
                auto parent = drawable->parent();
                auto index = m_Drawables.size() - 1;
                while (index >= 1)
                {
                    auto swappingDrawable = m_Drawables[index - 1];
                    std::swap(m_Drawables[index - 1], m_Drawables[index]);
                    if (swappingDrawable == parent)
                    {
                        break;
                    }
                    index--;
                }
            }

            for (ContainerComponent* parent = drawable; parent != nullptr;
                 parent = parent->parent())
            {
                auto itr = componentDrawRules.find(parent);
                if (itr != componentDrawRules.end())
                {
                    drawable->flattenedDrawRules = itr->second;
                    break;
                }
            }
        }
    }
    // Iterate over the drawables in order to inject proxies for layouts
    std::vector<LayoutComponent*> layouts;
    for (int i = 0; i < m_Drawables.size(); i++)
    {
        auto drawable = m_Drawables[i];
        LayoutComponent* currentLayout = nullptr;
        bool isInCurrentLayout = true;
        if (!layouts.empty())
        {
            currentLayout = layouts.back();
            isInCurrentLayout = drawable->isChildOfLayout(currentLayout);
        }
        // We inject a DrawableProxy after all of the children of a
        // LayoutComponent so that we can draw a stroke above and background
        // below the children This also allows us to clip the children
        if (currentLayout != nullptr && !isInCurrentLayout)
        {
            // This is the first item in the list of drawables that isn't a
            // child of the layout, so we insert a proxy before it
            do
            {
                m_Drawables.insert(m_Drawables.begin() + i,
                                   currentLayout->proxy());
                i += 1;
                layouts.pop_back();
                if (!layouts.empty())
                {
                    currentLayout = layouts.back();
                }
            } while (!layouts.empty() &&
                     !drawable->isChildOfLayout(currentLayout));
        }
        if (drawable->is<LayoutComponent>())
        {
            layouts.push_back(drawable->as<LayoutComponent>());
        }
    }
    while (!layouts.empty())
    {
        auto layout = layouts.back();
        m_Drawables.push_back(layout->proxy());
        layouts.pop_back();
    }

    sortDependencies();

    std::vector<DrawRules*> rulesList;
    // Build the rules in the right order. We use the map componentDrawRules
    // to make sure we traverse the objects in the right order from parent
    // to child, and add the rules accordingly.
    for (auto object : m_Objects)
    {
        if (object == nullptr)
        {
            continue;
        }
        auto itr = componentDrawRules.find(object);
        if (itr != componentDrawRules.end())
        {
            rulesList.emplace_back(componentDrawRules[object]);
        }
    }
    DrawTarget root;
    // Build up the draw order. Look for draw targets and build
    // their dependencies.
    for (auto rules : rulesList)
    {
        for (auto child : rules->children())
        {
            auto target = child->as<DrawTarget>();
            root.addDependent(target);
            auto dependentRules = target->drawable()->flattenedDrawRules;
            if (dependentRules != nullptr)
            {
                // Because we don't store targets on rules, we need
                // to find the targets that belong to this rule
                // here.
                for (auto object : m_Objects)
                {
                    if (object != nullptr && object->is<DrawTarget>())
                    {
                        DrawTarget* dependentTarget = object->as<DrawTarget>();
                        if (dependentTarget->parent() == dependentRules)
                        {
                            dependentTarget->addDependent(target);
                        }
                    }
                }
            }
        }
    }
    DependencySorter sorter;
    std::vector<Component*> drawTargetOrder;
    sorter.sort(&root, drawTargetOrder);
    auto itr = drawTargetOrder.begin();
    itr++;
    while (itr != drawTargetOrder.end())
    {
        m_DrawTargets.push_back(static_cast<DrawTarget*>(*itr++));
    }
    return StatusCode::Ok;
}

void Artboard::sortDrawOrder()
{
    m_drawOrderChangeCounter =
        m_drawOrderChangeCounter == std::numeric_limits<uint8_t>::max()
            ? 0
            : m_drawOrderChangeCounter + 1;
    for (auto target : m_DrawTargets)
    {
        target->first = target->last = nullptr;
    }

    m_FirstDrawable = nullptr;
    Drawable* lastDrawable = nullptr;
    for (auto drawable : m_Drawables)
    {
        auto rules = drawable->flattenedDrawRules;
        if (rules != nullptr && rules->activeTarget() != nullptr)
        {

            auto target = rules->activeTarget();
            if (target->first == nullptr)
            {
                target->first = target->last = drawable;
                drawable->prev = drawable->next = nullptr;
            }
            else
            {
                target->last->next = drawable;
                drawable->prev = target->last;
                target->last = drawable;
                drawable->next = nullptr;
            }
        }
        else
        {
            drawable->prev = lastDrawable;
            drawable->next = nullptr;
            if (lastDrawable == nullptr)
            {
                lastDrawable = m_FirstDrawable = drawable;
            }
            else
            {
                lastDrawable->next = drawable;
                lastDrawable = drawable;
            }
        }
    }

    for (auto rule : m_DrawTargets)
    {
        if (rule->first == nullptr)
        {
            continue;
        }
        auto targetDrawable = rule->drawable();
        switch (rule->placement())
        {
            case DrawTargetPlacement::before:
            {
                if (targetDrawable->prev != nullptr)
                {
                    targetDrawable->prev->next = rule->first;
                    rule->first->prev = targetDrawable->prev;
                }
                if (targetDrawable == m_FirstDrawable)
                {
                    m_FirstDrawable = rule->first;
                }
                targetDrawable->prev = rule->last;
                rule->last->next = targetDrawable;
                break;
            }
            case DrawTargetPlacement::after:
            {
                if (targetDrawable->next != nullptr)
                {
                    targetDrawable->next->prev = rule->last;
                    rule->last->next = targetDrawable->next;
                }
                if (targetDrawable == lastDrawable)
                {
                    lastDrawable = rule->last;
                }
                targetDrawable->next = rule->first;
                rule->first->prev = targetDrawable;
                break;
            }
        }
    }

    m_FirstDrawable = lastDrawable;
}

void Artboard::sortDependencies()
{
    DependencySorter sorter;
    sorter.sort(this, m_DependencyOrder);
    unsigned int graphOrder = 0;
    for (auto component : m_DependencyOrder)
    {
        component->m_GraphOrder = graphOrder++;
    }
    m_Dirt |= ComponentDirt::Components;
}

void Artboard::addObject(Core* object) { m_Objects.push_back(object); }

void Artboard::addAnimation(LinearAnimation* object)
{
    m_Animations.push_back(object);
}

void Artboard::addStateMachine(StateMachine* object)
{
    m_StateMachines.push_back(object);
}

void Artboard::addScriptedObject(ScriptedObject* object)
{
    m_ScriptedObjects.push_back(object);
}

Core* Artboard::resolve(uint32_t id) const
{
    if (id >= static_cast<int>(m_Objects.size()))
    {
        return nullptr;
    }
    return m_Objects[id];
}

uint32_t Artboard::idOf(Core* object) const
{
    auto it = std::find(m_Objects.begin(), m_Objects.end(), object);

    if (it != m_Objects.end())
    {
        return castTo<uint32_t>(it - m_Objects.begin());
    }
    else
    {
        return 0;
    }
}

void Artboard::onComponentDirty(Component* component)
{
    m_Dirt |= ComponentDirt::Components;

    /// If the order of the component is less than the current dirt
    /// depth, update the dirt depth so that the update loop can break
    /// out early and re-run (something up the tree is dirty).
    if (component->graphOrder() < m_DirtDepth)
    {
        m_DirtDepth = component->graphOrder();
    }
}

void Artboard::onDirty(ComponentDirt dirt)
{
    m_Dirt |= ComponentDirt::Components;
}

#ifdef WITH_RIVE_LAYOUT
void Artboard::propagateSize()
{
    addDirt(ComponentDirt::Path);
    if (sharesLayoutWithHost())
    {
        m_host->markHostTransformDirty();
    }
#ifdef WITH_RIVE_TOOLS
    if (m_layoutChangedCallback != nullptr)
    {
        m_layoutChangedCallback(callbackUserData);
    }
#endif
}
#endif

bool Artboard::sharesLayoutWithHost() const
{
    return m_host != nullptr && m_host->isLayoutProvider();
}

void Artboard::cloneObjectDataBinds(const Core* object,
                                    Core* clone,
                                    Artboard* artboard) const
{

    for (auto dataBind : dataBinds())
    {
        if (dataBind->target() == object)
        {
            auto dataBindClone = static_cast<DataBind*>(dataBind->clone());
            dataBindClone->target(clone);
            dataBindClone->file(dataBind->file());
            if (dataBind->converter() != nullptr)
            {

                dataBindClone->converter(
                    dataBind->converter()->clone()->as<DataConverter>());
            }
            artboard->addDataBind(dataBindClone);
        }
    }
}
void Artboard::host(ArtboardHost* artboardHost)
{
    m_host = artboardHost;
#ifdef WITH_RIVE_LAYOUT
    if (!sharesLayoutWithHost())
    {
        return;
    }
    Artboard* parent = parentArtboard();
    if (parent != nullptr)
    {
        parent->markLayoutDirty(this);
        parent->syncLayoutChildren();
    }
#endif
}

ArtboardHost* Artboard::host() const { return m_host; }

StatusCode Artboard::onAddedClean(CoreContext* context)
{
    auto code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    NodeBase::x(0);
    NodeBase::y(0);
    return StatusCode::Ok;
}

Artboard* Artboard::parentArtboard() const
{
    if (m_host == nullptr)
    {
        return nullptr;
    }
    return m_host->parentArtboard();
}

float Artboard::layoutWidth() const
{
#ifdef WITH_RIVE_LAYOUT
    return m_layout.width();
#else
    return width();
#endif
}

float Artboard::layoutHeight() const
{
#ifdef WITH_RIVE_LAYOUT
    return m_layout.height();
#else
    return height();
#endif
}

float Artboard::layoutX() const
{
#ifdef WITH_RIVE_LAYOUT
    return m_layout.left();
#else
    return 0.0f;
#endif
}

float Artboard::layoutY() const
{
#ifdef WITH_RIVE_LAYOUT
    return m_layout.top();
#else
    return 0.0f;
#endif
}

void Artboard::updateRenderPath()
{
    AABB bg = AABB::fromLTWH(-layoutWidth() * originX(),
                             -layoutHeight() * originY(),
                             layoutWidth(),
                             layoutHeight());
    AABB clip;
    if (m_FrameOrigin)
    {
        clip = {0.0f, 0.0f, layoutWidth(), layoutHeight()};
    }
    else
    {
        clip = bg;
    }
    m_localPath.rewind();
    m_localPath.addRect(bg);
    m_worldPath.rewind();
    m_worldPath.addRect(clip);
}

void Artboard::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::DrawOrder))
    {
        sortDrawOrder();
    }
#ifdef WITH_RIVE_LAYOUT
    if (hasDirt(value, ComponentDirt::LayoutStyle))
    {
        bool cascadeChanged = cascadeLayoutStyle(interpolation(),
                                                 interpolator(),
                                                 interpolationTime(),
                                                 actualDirection());
        // TODO: Explore whether we can remove the syncStyleChanges call in
        // updatePass. Since updatePass calls updateComponents, where the first
        // component is the artboard itself, hence calling update, we end up
        // calling this twice. Although it is safe, because syncStyleChanges
        // checks for the list of dirty layouts that would be empty at this
        // point, it seems redundant.
        syncStyleChangesWithUpdate(cascadeChanged);
    }
#endif
    m_hostTransformMarkedDirty = false;
}

void Artboard::addDirtyDataBind(DataBind* dataBind)
{
    onComponentDirty(dataBind->target()->as<Component>());
    DataBindContainer::addDirtyDataBind(dataBind);
}

void Artboard::updateDataBinds(bool applyTargetToSource)
{
    for (auto artboardHost : m_ArtboardHosts)
    {
        artboardHost->updateDataBinds();
    }
    DataBindContainer::updateDataBinds(applyTargetToSource);
}

bool Artboard::updateComponents()
{
    if (!hasDirt(ComponentDirt::Components))
    {
        return false;
    }
    const int maxSteps = 100;
    int step = 0;
    auto count = m_DependencyOrder.size();
    while (hasDirt(ComponentDirt::Components) && step < maxSteps)
    {
        m_Dirt = m_Dirt & ~ComponentDirt::Components;

        // Track dirt depth here so that if something else marks
        // dirty, we restart.
        for (unsigned int i = 0; i < count; i++)
        {
            auto component = m_DependencyOrder[i];
            m_DirtDepth = i;
            auto d = component->m_Dirt;
            if (d == ComponentDirt::None ||
                (d & ComponentDirt::Collapsed) == ComponentDirt::Collapsed)
            {
                continue;
            }
            component->m_Dirt = ComponentDirt::None;
            component->update(d);

            // If the update changed the dirt depth by adding dirt
            // to something before us (in the DAG), early out and
            // re-run the update.
            if (m_DirtDepth < i)
            {
                break;
            }
        }
        step++;
    }
    return true;
}

LayoutData* Artboard::takeLayoutData()
{
#ifdef WITH_RIVE_LAYOUT
    m_updatesOwnLayout = false;
    return m_layoutData;
#else
    return nullptr;
#endif
}

void Artboard::cleanLayout(LayoutComponent* layoutComponent)
{
    assert(!m_isCleaningDirtyLayouts);
    if (m_isCleaningDirtyLayouts)
    {
        fprintf(stderr,
                "Artboard::cleanLayout - trying to remove a dirty layout "
                "during clean pass!\n");
        return;
    }

    if (!m_dirtyLayout.empty())
    {
        auto itr = m_dirtyLayout.find(layoutComponent);
        if (itr != m_dirtyLayout.end())
        {
            m_dirtyLayout.erase(itr);
        }
    }
    // If we called cleanLayout on ourselves, make sure to also
    // call it on our parent artboard in case we were dirtied
    if (layoutComponent == this)
    {
        Artboard* parent = parentArtboard();
        if (parent != nullptr)
        {
            parent->cleanLayout(layoutComponent);
        }
    }
}

void Artboard::markLayoutDirty(LayoutComponent* layoutComponent)
{
    assert(!m_isCleaningDirtyLayouts);
    if (m_isCleaningDirtyLayouts)
    {
        fprintf(stderr,
                "Artboard::markLayoutDirty - trying to mark a layout dirty "
                "during clean pass!\n");
        return;
    }
#ifdef WITH_RIVE_TOOLS
    if (m_dirtyLayout.empty() && m_layoutDirtyCallback != nullptr)
    {
        m_layoutDirtyCallback(callbackUserData);
    }
#endif
    m_dirtyLayout.insert(layoutComponent);
    if (isInstance())
    {
        if (sharesLayoutWithHost())
        {
            m_host->markHostingLayoutDirty(this->as<ArtboardInstance>());
        }
        else
        {
            markHostTransformDirty();
        }
    }
    addDirt(ComponentDirt::Components);
}

void Artboard::markHostTransformDirty()
{
#ifdef WITH_RIVE_TOOLS
    if (!m_hostTransformMarkedDirty && m_transformDirtyCallback != nullptr)
    {
        m_transformDirtyCallback(callbackUserData);
    }
#endif
    m_hostTransformMarkedDirty = true;
    if (host())
    {
        host()->markHostTransformDirty();
    }
}

void Artboard::syncStyleChangesWithUpdate(bool forceUpdate)
{
#ifdef WITH_RIVE_LAYOUT
    if (syncStyleChanges() && (m_updatesOwnLayout || forceUpdate))
    {
        calculateLayout();
        updateLayoutBounds(/*animation*/ true); // maybe use a static to allow
                                                // the editor to set this.
    }
#endif
}

bool Artboard::syncStyleChanges()
{
    bool updated = false;
    m_isCleaningDirtyLayouts = true;
#ifdef WITH_RIVE_LAYOUT
    if (!m_dirtyLayout.empty())
    {
        for (auto layout : m_dirtyLayout)
        {
            if (layout == nullptr)
            {
                continue;
            }
            switch (layout->coreType())
            {
                case ArtboardBase::typeKey:
                {
                    auto artboard = layout->as<Artboard>();
                    if (artboard == this)
                    {
                        artboard->syncStyle();
                    }
                    else
                    {
                        // This is a nested artboard, sync its changes too.
                        if (!artboard->updatesOwnLayout())
                        {
                            artboard->syncStyleChanges();
                        }
                    }
                    break;
                }

                default:
                    layout->syncStyle();
                    break;
            }
        }
        m_dirtyLayout.clear();
        updated = true;
    }
#endif
    m_isCleaningDirtyLayouts = false;
    return updated;
}

void Artboard::calculateLayout()
{
#if defined(WITH_RIVE_TOOLS) && !defined(TESTING)
    calculateLayoutInternal(NAN, NAN);
#else
    // If we're a child of another artboard (ie nested or artboard list item)
    // pass NAN so we compute our hugged size if applicable
    if (parentArtboard() != nullptr && m_updatesOwnLayout)
    {
        calculateLayoutInternal(NAN, NAN);
    }
    else
    {
        calculateLayoutInternal(width(), height());
    }
#endif
}

bool Artboard::updatePass(bool isRoot)
{
    updateDataBinds();
    bool didUpdate = false;
    syncStyleChangesWithUpdate();
    m_hostTransformMarkedDirty = false;

    if (m_JoysticksApplyBeforeUpdate)
    {
        for (auto joystick : m_Joysticks)
        {
            joystick->apply(this);
        }
    }
    if (updateComponents())
    {
        didUpdate = true;
    }
    if (!m_JoysticksApplyBeforeUpdate)
    {
        for (auto joystick : m_Joysticks)
        {
            if (!joystick->canApplyBeforeUpdate())
            {
                updateDataBinds();
                if (updateComponents())
                {
                    didUpdate = true;
                }
            }
            joystick->apply(this);
        }
        updateDataBinds();
        if (updateComponents())
        {
            didUpdate = true;
        }
    }
    if (didUpdate)
    {
        updateDataBinds();
    }
    return didUpdate;
}

bool Artboard::advanceInternal(float elapsedSeconds, AdvanceFlags flags)
{
    bool didUpdate = false;

    for (auto dep : m_DependencyOrder)
    {
        auto adv = AdvancingComponent::from(dep);
        if (adv != nullptr && adv->advanceComponent(elapsedSeconds, flags))
        {
            didUpdate = true;
        }
    }
    if (advanceDataBinds(elapsedSeconds))
    {
        didUpdate = true;
    }

    return didUpdate;
}

void Artboard::reset()
{
    for (auto obj : m_Resettables)
    {
        obj->reset();
    }
}

bool Artboard::advance(float elapsedSeconds, AdvanceFlags flags)
{
    AdvanceFlags advancingFlags = flags;
    advancingFlags |= AdvanceFlags::IsRoot;
    bool didUpdate = advanceInternal(elapsedSeconds, advancingFlags);
    if (updatePass(true))
    {
        didUpdate = true;
    }
    return didUpdate || hasDirt(ComponentDirt::Components);
}

Core* Artboard::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    if (clip())
    {
        // TODO: can we get the rawpath for the clip?
    }

    auto mx = xform;
    if (m_FrameOrigin)
    {
        mx *= Mat2D::fromTranslate(layoutWidth() * originX(),
                                   layoutHeight() * originY());
    }

    Drawable* last = m_FirstDrawable;
    if (last)
    {
        // walk to the end, so we can visit in reverse-order
        while (last->prev)
        {
            last = last->prev;
        }
    }
    for (auto drawable = last; drawable; drawable = drawable->next)
    {
        if (drawable->isHidden())
        {
            continue;
        }
        if (auto c = drawable->hitTest(hinfo, mx))
        {
            return c;
        }
    }

    // TODO: should we hit-test the background?

    return nullptr;
}

Vec2D Artboard::rootTransform(const Vec2D& point)
{
    if (host())
    {
        return host()->hostTransformPoint(point, this->as<ArtboardInstance>());
    }
#ifdef WITH_RIVE_TOOLS
    // Editor artboards don't have a host, so we expose a function that calls
    // the host in dart.
    if (m_rootTransformCallback != nullptr)
    {
        auto x =
            m_rootTransformCallback(callbackUserData, point.x, point.y, true);
        auto y =
            m_rootTransformCallback(callbackUserData, point.x, point.y, false);
        return Vec2D(x, y);
    }
#endif
    return point;
}

bool Artboard::hitTestPoint(const Vec2D& position,
                            bool skipOnUnclipped,
                            bool isPrimaryHit)
{
    if (host() != nullptr && isInstance())
    {
        if (!host()->hitTestHost(position,
                                 skipOnUnclipped,
                                 this->as<ArtboardInstance>()))
        {
            return false;
        }
    }
#ifdef WITH_RIVE_TOOLS
    // Editor artboards don't have a host, so we expose a function that calls
    // the host in dart.
    if (m_testBoundsCallback != nullptr)
    {
        // Dart can't return booleans to cpp, so we use a uint_8 instead
        auto didHit = m_testBoundsCallback(callbackUserData,
                                           position.x,
                                           position.y,
                                           skipOnUnclipped);
        if (didHit == 0)
        {
            return false;
        }
    }
#endif
    return LayoutComponent::hitTestPoint(position,
                                         skipOnUnclipped,
                                         isPrimaryHit);
}

void Artboard::draw(Renderer* renderer) { draw(renderer, DrawOption::kNormal); }

void Artboard::draw(Renderer* renderer, DrawOption option)
{
    RIVE_PROF_SCOPE()

    if (renderOpacity() == 0)
    {
        return;
    }
    bool save = clip() || m_FrameOrigin;
    if (save)
    {
        renderer->save();
    }
    if (clip())
    {
        renderer->clipPath(m_worldPath.renderPath(this));
    }

    if (m_FrameOrigin)
    {
        Mat2D artboardTransform;
        artboardTransform[4] = layoutWidth() * originX();
        artboardTransform[5] = layoutHeight() * originY();
        renderer->transform(artboardTransform);
    }

    if (option != DrawOption::kHideBG)
    {
        for (auto shapePaint : m_ShapePaints)
        {
            if (!shapePaint->shouldDraw())
            {
                continue;
            }
            auto shapePaintPath = shapePaint->pickPath(this);
            if (shapePaintPath == nullptr)
            {
                continue;
            }
            shapePaint->draw(renderer, shapePaintPath, worldTransform());
        }
    }

    if (option != DrawOption::kHideFG)
    {
        for (auto drawable = m_FirstDrawable; drawable != nullptr;
             drawable = drawable->prev)
        {
            if (drawable->isHidden())
            {
                continue;
            }
            drawable->draw(renderer);
        }
    }
    if (save)
    {
        renderer->restore();
    }
}

void Artboard::addToRenderPath(RenderPath* path, const Mat2D& transform)
{
    for (auto drawable = m_FirstDrawable; drawable != nullptr;
         drawable = drawable->prev)
    {
        if (drawable->isHidden() || !drawable->is<Shape>())
        {
            continue;
        }
        Shape* shape = drawable->as<Shape>();
        shape->addToRenderPath(path, transform);
    }
}

void Artboard::addToRawPath(RawPath& path, const Mat2D* transform)
{
    for (auto drawable = m_FirstDrawable; drawable != nullptr;
         drawable = drawable->prev)
    {
        if (drawable->isHidden() || !drawable->is<Shape>())
        {
            continue;
        }
        Shape* shape = drawable->as<Shape>();
        shape->addToRawPath(path, transform);
    }
}

Vec2D Artboard::origin() const
{
    return m_FrameOrigin
               ? Vec2D(0.0f, 0.0f)
               : Vec2D(-layoutWidth() * originX(), -layoutHeight() * originY());
}

void Artboard::xChanged()
{
    Super::xChanged();
    markHostTransformDirty();
}

void Artboard::yChanged()
{
    Super::yChanged();
    markHostTransformDirty();
}

AABB Artboard::bounds() const
{
    return m_FrameOrigin ? AABB(0.0f, 0.0f, layoutWidth(), layoutHeight())
                         : AABB::fromLTWH(-layoutWidth() * originX(),
                                          -layoutHeight() * originY(),
                                          layoutWidth(),
                                          layoutHeight());
}

AABB Artboard::worldBounds() const
{
    return AABB::fromLTWH(NodeBase::x(),
                          NodeBase::y(),
                          m_layout.width(),
                          m_layout.height());
}

bool Artboard::isTranslucent() const
{
    for (const auto sp : m_ShapePaints)
    {
        if (!sp->isTranslucent())
        {
            return false; // one opaque fill is sufficient to be opaque
        }
    }
    return true;
}

bool Artboard::hasAudio() const
{
    for (auto object : m_Objects)
    {
        if (object != nullptr && object->coreType() == AudioEventBase::typeKey)
        {
            return true;
        }
    }
    for (auto artboardHost : m_ArtboardHosts)
    {
        for (int i = 0; i < artboardHost->artboardCount(); i++)
        {
            auto artboard = artboardHost->artboardInstance(i);
            if (artboard != nullptr && artboard->hasAudio())
            {
                return true;
            }
        }
    }
    return false;
}

bool Artboard::isTranslucent(const LinearAnimation* anim) const
{
    // For now we're conservative/lazy -- if we see that any of our paints are
    // animated we assume that might make it non-opaque, so we early out
    for (const auto& obj : anim->m_KeyedObjects)
    {
        const auto ptr = this->resolve(obj->objectId());
        for (const auto sp : m_ShapePaints)
        {
            if (ptr == sp)
            {
                return true;
            }
        }
    }

    // If we get here, we have no animations, so just check our paints for
    // opacity
    return this->isTranslucent();
}

bool Artboard::isTranslucent(const LinearAnimationInstance* inst) const
{
    return this->isTranslucent(inst->animation());
}

std::string Artboard::animationNameAt(size_t index) const
{
    auto la = this->animation(index);
    return la ? la->name() : "";
}

std::string Artboard::stateMachineNameAt(size_t index) const
{
    auto sm = this->stateMachine(index);
    return sm ? sm->name() : "";
}

LinearAnimation* Artboard::animation(const std::string& name) const
{
    for (auto animation : m_Animations)
    {
        if (animation->name() == name)
        {
            return animation;
        }
    }
    return nullptr;
}

LinearAnimation* Artboard::animation(size_t index) const
{
    if (index >= m_Animations.size())
    {
        return nullptr;
    }
    return m_Animations[index];
}

StateMachine* Artboard::stateMachine(const std::string& name) const
{
    for (auto machine : m_StateMachines)
    {
        if (machine->name() == name)
        {
            return machine;
        }
    }
    return nullptr;
}

StateMachine* Artboard::stateMachine(size_t index) const
{
    if (index >= m_StateMachines.size())
    {
        return nullptr;
    }
    return m_StateMachines[index];
}

int Artboard::defaultStateMachineIndex() const
{
    int index = defaultStateMachineId();
    if ((size_t)index >= m_StateMachines.size())
    {
        index = -1;
    }
    return index;
}

NestedArtboard* Artboard::nestedArtboard(const std::string& name) const
{
    for (auto nested : m_NestedArtboards)
    {
        if (nested->name() == name)
        {
            return nested;
        }
    }
    return nullptr;
}

NestedArtboard* Artboard::nestedArtboardAtPath(const std::string& path) const
{
    // name parameter can be a name or a path to recursively find a nested
    // artboard
    std::string delimiter = "/";
    size_t firstDelim = path.find(delimiter);
    std::string artboardName =
        firstDelim == std::string::npos ? path : path.substr(0, firstDelim);
    std::string restOfPath = firstDelim == std::string::npos
                                 ? ""
                                 : path.substr(firstDelim + 1, path.size());

    // Find the nested artboard at this level
    if (!artboardName.empty())
    {
        auto nested = nestedArtboard(artboardName);
        if (nested != nullptr)
        {
            if (restOfPath.empty())
            {
                return nested;
            }
            else
            {
                auto artboard = nested->artboardInstance();
                return artboard->nestedArtboardAtPath(restOfPath);
            }
        }
    }
    return nullptr;
}

// std::unique_ptr<ArtboardInstance> Artboard::instance() const
// {
//     std::unique_ptr<ArtboardInstance> artboardClone(new ArtboardInstance);
//     artboardClone->copy(*this);

//     artboardClone->m_Factory = m_Factory;
//     artboardClone->m_FrameOrigin = m_FrameOrigin;
//     artboardClone->m_IsInstance = true;

//     std::vector<Core*>& cloneObjects = artboardClone->m_Objects;
//     cloneObjects.push_back(artboardClone.get());

//     if (!m_Objects.empty())
//     {
//         // Skip first object (artboard).
//         auto itr = m_Objects.begin();
//         while (++itr != m_Objects.end())
//         {
//             auto object = *itr;
//             cloneObjects.push_back(object == nullptr ? nullptr :
//             object->clone());
//         }
//     }

//     for (auto animation : m_Animations)
//     {
//         artboardClone->m_Animations.push_back(animation);
//     }
//     for (auto stateMachine : m_StateMachines)
//     {
//         artboardClone->m_StateMachines.push_back(stateMachine);
//     }

//     if (artboardClone->initialize() != StatusCode::Ok)
//     {
//         artboardClone = nullptr;
//     }

//     assert(artboardClone->isInstance());
//     return artboardClone;
// }

void Artboard::frameOrigin(bool value)
{
    if (value == m_FrameOrigin)
    {
        return;
    }
    m_FrameOrigin = value;
    addDirt(ComponentDirt::Path);
}

StatusCode Artboard::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    StatusCode result = Super::import(importStack);
    if (result == StatusCode::Ok)
    {
        backboardImporter->addArtboard(this);
    }
    else
    {
        backboardImporter->addMissingArtboard();
    }
    return result;
}

void Artboard::internalDataContext(DataContext* value)
{
    m_DataContext = value;
    for (auto artboardHost : m_ArtboardHosts)
    {
        auto value = m_DataContext->getViewModelInstance(
            artboardHost->dataBindPathIds());
        if (value != nullptr && value->is<ViewModelInstance>())
        {
            artboardHost->bindViewModelInstance(value, m_DataContext);
        }
        else
        {
            artboardHost->internalDataContext(m_DataContext);
        }
    }
    bindDataBindsFromContext(m_DataContext);
    sortDataBinds();

    for (auto obj : m_ScriptedObjects)
    {
        obj->reinit();
    }
}

void Artboard::rebind() { internalDataContext(m_DataContext); }

void Artboard::unbind()
{
    clearDataContext();
    unbindDataBinds();
    for (auto artboardHost : m_ArtboardHosts)
    {
        artboardHost->unbind();
    }
}

void Artboard::clearDataContext()
{
    if (m_DataContext)
    {
        if (m_ownsDataContext)
        {
            m_DataContext->viewModelInstance()->removeDependent(this);
            delete m_DataContext;
        }
        m_DataContext = nullptr;
    }
    m_ownsDataContext = false;
    for (auto artboardHost : m_ArtboardHosts)
    {
        artboardHost->clearDataContext();
    }
}

float Artboard::volume() const { return m_volume; }
void Artboard::volume(float value)
{
    m_volume = value;
    for (auto artboardHost : m_ArtboardHosts)
    {
        for (int i = 0; i < artboardHost->artboardCount(); i++)
        {
            auto artboard = artboardHost->artboardInstance(i);
            if (artboard != nullptr)
            {
                artboard->volume(value);
            }
        }
    }
}

void Artboard::dataContext(DataContext* value) { internalDataContext(value); }

void Artboard::bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance)
{
    bindViewModelInstance(viewModelInstance, nullptr);
}

void Artboard::bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                                     DataContext* parent)
{
    if (viewModelInstance == nullptr)
    {
        unbind();
        return;
    }
    clearDataContext();
    m_ownsDataContext = true;
    auto dataContext = new DataContext(viewModelInstance);
    if (dataContext->viewModelInstance())
    {
        dataContext->viewModelInstance()->addDependent(this);
    }
    dataContext->parent(parent);
    internalDataContext(dataContext);
}

bool Artboard::isAncestor(const Artboard* artboard)
{
    if (artboard != nullptr && m_artboardSource == artboard->artboardSource())
    {
        return true;
    }
    if (parentArtboard() != nullptr)
    {
        return parentArtboard()->isAncestor(artboard);
    }
#ifdef WITH_RIVE_TOOLS
    // Editor artboards don't have a host, so we expose a function that calls
    // the host in dart.
    if (m_isAncestorCallback != nullptr)
    {
        // Dart can't return booleans to cpp, so we use a uint_8 instead
        auto isAncestor =
            m_isAncestorCallback(callbackUserData, artboard->artboardId());
        if (isAncestor == 1)
        {
            return true;
        }
    }
#endif
    return false;
}

////////// ArtboardInstance

#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"

ArtboardInstance::ArtboardInstance() {}

ArtboardInstance::~ArtboardInstance() {}

std::unique_ptr<LinearAnimationInstance> ArtboardInstance::animationAt(
    size_t index)
{
    auto la = this->animation(index);
    return la ? rivestd::make_unique<LinearAnimationInstance>(la, this)
              : nullptr;
}

std::unique_ptr<LinearAnimationInstance> ArtboardInstance::animationNamed(
    const std::string& name)
{
    auto la = this->animation(name);
    return la ? rivestd::make_unique<LinearAnimationInstance>(la, this)
              : nullptr;
}

std::unique_ptr<StateMachineInstance> ArtboardInstance::stateMachineAt(
    size_t index)
{
    auto sm = this->stateMachine(index);
    return sm ? rivestd::make_unique<StateMachineInstance>(sm, this) : nullptr;
}

std::unique_ptr<StateMachineInstance> ArtboardInstance::stateMachineNamed(
    const std::string& name)
{
    auto sm = this->stateMachine(name);
    return sm ? rivestd::make_unique<StateMachineInstance>(sm, this) : nullptr;
}

std::unique_ptr<StateMachineInstance> ArtboardInstance::defaultStateMachine()
{
    const int index = this->defaultStateMachineIndex();
    return index >= 0 ? this->stateMachineAt(index) : nullptr;
}

std::unique_ptr<Scene> ArtboardInstance::defaultScene()
{
    std::unique_ptr<Scene> scene = this->defaultStateMachine();
    if (!scene)
    {
        scene = this->stateMachineAt(0);
    }
    if (!scene)
    {
        scene = this->animationAt(0);
    }
    return scene;
}

SMIInput* ArtboardInstance::input(const std::string& name,
                                  const std::string& path)
{
    return getNamedInput<SMIInput>(name, path);
}

template <typename InstType>
InstType* ArtboardInstance::getNamedInput(const std::string& name,
                                          const std::string& path)
{
    if (!path.empty())
    {
        auto nestedArtboard = nestedArtboardAtPath(path);
        if (nestedArtboard != nullptr)
        {
            auto input = nestedArtboard->input(name);
            if (input != nullptr && input->input() != nullptr)
            {
                return static_cast<InstType*>(input->input());
            }
        }
    }
    return nullptr;
}

SMIBool* ArtboardInstance::getBool(const std::string& name,
                                   const std::string& path)
{
    return getNamedInput<SMIBool>(name, path);
}

SMINumber* ArtboardInstance::getNumber(const std::string& name,
                                       const std::string& path)
{
    return getNamedInput<SMINumber>(name, path);
}
SMITrigger* ArtboardInstance::getTrigger(const std::string& name,
                                         const std::string& path)
{
    return getNamedInput<SMITrigger>(name, path);
}

TextValueRun* ArtboardInstance::getTextRun(const std::string& name,
                                           const std::string& path)
{
    if (path.empty())
    {
        return nullptr;
    }

    auto nestedArtboard = nestedArtboardAtPath(path);
    if (nestedArtboard == nullptr)
    {
        return nullptr;
    }

    auto artboardInstance = nestedArtboard->artboardInstance();
    if (artboardInstance == nullptr)
    {
        return nullptr;
    }

    return artboardInstance->find<TextValueRun>(name);
}

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
rcp<AudioEngine> Artboard::audioEngine() const { return m_audioEngine; }
void Artboard::audioEngine(rcp<AudioEngine> audioEngine)
{
    m_audioEngine = audioEngine;
    for (auto artboardHost : m_ArtboardHosts)
    {
        for (int i = 0; i < artboardHost->artboardCount(); i++)
        {
            auto artboard = artboardHost->artboardInstance(i);
            if (artboard != nullptr)
            {
                artboard->audioEngine(audioEngine);
            }
        }
    }
}
#endif
