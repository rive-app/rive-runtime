#include "rive/artboard.hpp"
#include "rive/file.hpp"
#ifdef WITH_RIVE_SCRIPTING_WASM
#include "rive/wasm/wasm_scripting_vm.hpp"
#endif
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/backboard.hpp"
#include "rive/component.hpp"
#include "rive/container_component.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive/semantic/semantic_manager.hpp"
#include "rive/semantic/semantic_node.hpp"
#include "rive/input/focusable.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/custom_property_color.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_trigger.hpp"
#include "rive/math/math_types.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/dependency_sorter.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/draw_rules.hpp"
#include "rive/draw_target.hpp"
#include "rive/audio_event.hpp"
#include "rive/draw_target_placement.hpp"
#include "rive/drawable.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/watermark.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/layout_component.hpp"
#include "rive/node.hpp"
#include "rive/foreground_layout_drawable.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/joystick.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind_flags.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/view_model_type.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_variation_helper.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/event.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/profiler/profiler_macros.h"
#include "rive/scripted/scripted_object.hpp"
#ifdef WITH_RIVE_SCRIPTING
#ifdef WITH_RIVE_SCRIPTING_LUAU
#include "rive/lua/rive_lua_libs.hpp"
#endif
#endif
#include "rive/async/work_pool.hpp"
#include "rive/bitmap_cache.hpp"
#include "rive/layer_mask.hpp"
#ifdef RIVE_CANVAS
#include "rive/offscreen_raster.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#endif

#include <set>
#include <unordered_map>

using namespace rive;

uint64_t Artboard::sm_frameId = 0;
#ifdef TESTING
uint64_t Artboard::sm_layoutPassCount = 0;
uint64_t Artboard::sm_dirtNotifications = 0;
#endif

Artboard::Artboard()
{
    // Artboards need to override default clip value to true.
    m_Clip = true;
#ifdef WITH_RIVE_TOOLS
    callbackUserData = this;
#endif
}

#ifdef TESTING
Artboard::Artboard(Factory* factory) : m_Factory(factory) { m_Clip = true; }
#endif

Artboard::~Artboard()
{
    // Tear the focus tree down FIRST, while every component is still alive.
    // Detaching a focused node clears focus, and that runs blur callbacks
    // (FocusData::blurred walks parents and siblings) — which must not fire
    // once the m_Objects loop below has started deleting those siblings.
    //
    // Only when we own the manager: an adopted one belongs to a host artboard
    // or to Dart, either of which may already be gone by the time a finalizer
    // frees us, so m_activeFocusManager is not safe to touch. Those artboards
    // are cleaned up explicitly by whoever adopted them —
    // NestedArtboard::updateArtboard and ArtboardComponentList::removeArtboard
    // both call cleanupFocusTree() before teardown — and a host being torn
    // down has already run this same block against the shared manager, so
    // nothing in a nested subtree is still focused by the time we reach it.
    if (m_ownedFocusManager != nullptr &&
        m_activeFocusManager == m_ownedFocusManager.get())
    {
#ifdef WITH_RIVE_TOOLS
        m_ownedFocusManager->setFocusChangedCallback(nullptr);
        m_ownedFocusManager->setScrollIntoViewCallback(nullptr);
#endif
        cleanupFocusTree();
    }

#ifdef WITH_RIVE_AUDIO
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    auto audioEngine = m_audioEngine != nullptr
                           ? m_audioEngine
                           : AudioEngine::RuntimeEngine(false);
#else
    auto audioEngine = AudioEngine::RuntimeEngine(false);
#endif
    if (audioEngine)
    {
        audioEngine->stop(this);
    }
#endif
    unbind();

    // ViewModelInstance and ViewModelInstanceValue inherit from RefCnt.
    //
    // ViewModelInstance (VMI) ownership: the artboard releases every VMI in
    // its m_Objects via unref(). NestedArtboard borrows its stateful child VMI
    // without bumping the refcount; dynamically-created bound VMIs are not in
    // m_Objects and are released by NestedArtboard itself.
    //
    // ViewModelInstanceValue (VMV) ownership: always owned by their parent
    // ViewModelInstance via rcp<> in m_PropertyValues. When VMI is deleted,
    // its destructor clears m_PropertyValues, which unrefs and deletes VMVs.
    //
    // Strategy:
    // 1) Identify VMI/VMV objects by pointer (safe is<>() check BEFORE any
    // deletions).
    // 2) Delete everything else immediately, deferring VMI unref until after
    // hierarchy components are gone.
    std::set<Core*> vmObjects;
    std::set<ViewModelInstance*> deferredVmiUnrefs;

    // First pass: identify ViewModelInstance and ViewModelInstanceValue
    // objects while memory is valid (before any deletions). Precompute which
    // VMIs should be released so we never dereference pointers after deletes.
    // Also detach FocusData from their nodes: with an adopted manager the
    // focus tree above wasn't torn down, and deleting a focused node blurs the
    // chain, which must not call into components already deleted.
    auto preDeletePass = [&](Core* object) {
        if (object == nullptr || object == this)
        {
            return;
        }
        if (object->is<FocusData>())
        {
            object->as<FocusData>()->detachFocusable();
            return;
        }
        if (object->is<ViewModelInstance>())
        {
            vmObjects.insert(object);
            deferredVmiUnrefs.insert(object->as<ViewModelInstance>());
            return;
        }
        if (object->is<ViewModelInstanceValue>())
        {
            vmObjects.insert(object);
        }
    };
    for (auto object : m_Objects)
    {
        preDeletePass(object);
    }
    for (auto object : m_invalidObjects)
    {
        preDeletePass(object);
    }

    auto isVmObject = [&](Core* object) -> bool {
        return vmObjects.count(object) != 0;
    };

    // Second pass: delete non-VM objects.
    for (auto object : m_Objects)
    {
        if (object == nullptr || object == this)
        {
            continue;
        }
        if (isVmObject(object))
        {
            continue;
        }
        delete object;
    }
    for (auto object : m_invalidObjects)
    {
        if (object == nullptr)
        {
            continue;
        }
        if (isVmObject(object))
        {
            continue;
        }
        delete object;
    }

    // Now release deferred ViewModelInstances after hierarchy components have
    // been destroyed. Releasing via unref() keeps RefCnt ownership semantics
    // intact and cascades to VMVs via m_PropertyValues rcps.
    for (auto* vmi : deferredVmiUnrefs)
    {
        vmi->unref();
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
                // Instead of immediately deleting invalid objects, we keep them
                // around in case other objects are referencing them. One
                // example is the backboard_importer keeping a reference on its
                // m_FileAssetReferencers. So the invalid objects are taken out
                // of the objects list but only deleted when the artboard is
                // destroyed.
                m_invalidObjects.push_back(m_Objects[i]);
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

void Artboard::reinstanceNestedArtboards(Factory* factory)
{
    for (auto object : m_Objects)
    {
        if (object == nullptr || !object->is<NestedArtboard>())
        {
            continue;
        }
        auto nested = object->as<NestedArtboard>();
        Artboard* current = nested->sourceArtboard();
        if (current == nullptr || !current->isInstance() ||
            current->m_artboardSource == nullptr)
        {
            continue;
        }
        auto replacement =
            current->m_artboardSource->instance<ArtboardInstance>(factory);
        if (replacement != nullptr)
        {
            nested->referencedArtboard(replacement.release());
        }
    }
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

#ifdef WITH_RIVE_EDITOR
    // Editor builds move typed-child registration (paths on shapes,
    // nested animations on NestedArtboard, skins, constraints, ...) out
    // of onAdded* and into editorParentChanged, driven by the coop
    // hydration passes. initialize() only ever runs for .riv-imported
    // artboards, which never see those passes — run the registration
    // sweep here or the typed lists stay empty (frozen rigs, zero
    // bounds, unresolvable nested inputs). Must happen BEFORE the
    // onAddedClean walk below: consumers like NestedArtboard mount
    // their nested state machines from the typed lists inside
    // onAddedClean. Parent pointers and child lists are wired by the
    // onAddedDirty pass above. Registrations are contractually
    // idempotent on `to`, so the pump's hydrateRuntimeArtboard doing
    // this again is harmless.
    for (auto object : m_Objects)
    {
        if (object != nullptr && object->is<Component>())
        {
            auto* component = object->as<Component>();
            if (auto* componentParent = component->parent())
            {
                component->editorParentChanged(nullptr, componentParent);
            }
        }
    }
#endif

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
                            "component width id %u\n",
                            static_cast<uint32_t>(rules->parentId()));
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
            case BitmapCacheBase::typeKey:
                m_BitmapCache = object->as<BitmapCache>();
                break;
        }
        auto advancingComponent = AdvancingComponent::from(object);
        if (advancingComponent)
        {
            m_advancingComponents.push_back(advancingComponent);
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
        else if (object->is<ClippingShape>())
        {
            m_clippingShapes.push_back(object->as<ClippingShape>());
        }
        else if (object->is<LayerMask>())
        {
            m_layerMasks.push_back(object->as<LayerMask>());
        }
    }
    // A layout only needs a DrawableProxy if it paints, clips, can gain a clip
    // at runtime, or is an interaction/listener hit target. Those deferred
    // reasons are stamped as ForceDrawableProxy when their references resolve
    // (KeyedObject::onAddedDirty, DataBind::target, ScrollConstraint, and
    // StateMachineListener::onAddedClean; source-only ones are carried to
    // instances by LayoutComponent::clone) -- guaranteeing the proxy exists
    // before this first, one-time injection. See needsDrawableProxy.

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
        // below the children This also allows us to clip the children. Layouts
        // that will never paint or clip skip the proxy entirely (it would draw
        // nothing) -- pure containers are the common case.
        if (currentLayout != nullptr && !isInCurrentLayout)
        {
            // This is the first item in the list of drawables that isn't a
            // child of the layout, so we insert a proxy before it
            do
            {
                if (currentLayout->needsDrawableProxy())
                {
                    m_Drawables.insert(m_Drawables.begin() + i,
                                       currentLayout->proxy());
                    i += 1;
                }
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
        if (layout->needsDrawableProxy())
        {
            m_Drawables.push_back(layout->proxy());
        }
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
    if (drawTargetOrder.size() > 0)
    {
        auto itr = drawTargetOrder.begin();
        itr++;
        while (itr != drawTargetOrder.end())
        {
            m_DrawTargets.push_back(static_cast<DrawTarget*>(*itr++));
        }
    }

    initScriptedObjects();
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

    // Interleave clipping operations between drawables that share the same
    // common clippings. Each clipping operation has a start and an end.
    for (auto& clippingShape : m_clippingShapes)
    {
        clippingShape->resetDrawables();
    }
    Drawable* currentDrawable = m_FirstDrawable;
    Drawable* nextDrawable = nullptr;
    std::vector<ClippingShape*> _clippingStack;
    while (currentDrawable)
    {
        currentDrawable->needsSaveOperation(true);
        const auto& drawableClippingShapes = currentDrawable->clippingShapes();
        // Remove all clippings that are not part of the current drawable. Since
        // they are applied as a stack, if one clipping is removed, all
        // subsequent clippings from the stack need to be removed as well
        size_t removingIndex = _clippingStack.size();
        for (size_t i = 0; i < _clippingStack.size(); ++i)
        {
            auto& cl = _clippingStack[i];
            // Check if this clipping should stay (is in both stack and
            // drawable's clippings)
            bool shouldStay = std::find(drawableClippingShapes.begin(),
                                        drawableClippingShapes.end(),
                                        cl) != drawableClippingShapes.end();
            if (!shouldStay)
            {
                removingIndex = i;
                break;
            }
        }

        // Remove all clippings from stack in the reverse order they were added
        if (_clippingStack.size() > 0 && removingIndex < _clippingStack.size())
        {

            size_t i = _clippingStack.size() - 1;
            while (i >= removingIndex)
            {
                auto& clippingShape = _clippingStack[i];
                // Insert in the drawing list a clipEnd for each clipping that
                // is removed
                auto proxyDrawable =
                    clippingShape->createProxyDrawable(&clippingShape->clipEnd);
                if (nextDrawable)
                {
                    proxyDrawable->next = nextDrawable;
                    nextDrawable->prev = proxyDrawable;
                }
                else
                {
                    fprintf(stderr,
                            "Error - adding clip end as first operation\n");
                }
                proxyDrawable->prev = currentDrawable;
                currentDrawable->next = proxyDrawable;
                nextDrawable = proxyDrawable;
                if (i == 0)
                {
                    break;
                }
                i--;
            }
            _clippingStack.erase(_clippingStack.begin() + removingIndex,
                                 _clippingStack.end());
        }
        // Find clippings that are applied to the drawable but are not on the
        // stack
        for (auto& clippingShape : drawableClippingShapes)
        {
            auto itr = std::find(_clippingStack.begin(),
                                 _clippingStack.end(),
                                 clippingShape);
            if (itr == _clippingStack.end())
            {
                auto proxyDrawable = clippingShape->createProxyDrawable(
                    &clippingShape->clipStart);
                if (nextDrawable)
                {
                    proxyDrawable->next = nextDrawable;
                    nextDrawable->prev = proxyDrawable;
                }
                else
                {
                    m_FirstDrawable = proxyDrawable;
                }
                proxyDrawable->prev = currentDrawable;
                currentDrawable->next = proxyDrawable;
                nextDrawable = proxyDrawable;
                _clippingStack.push_back(clippingShape);
            }
        }
        nextDrawable = currentDrawable;
        currentDrawable = currentDrawable->prev;
    }
    // Add closing calls to remaining clippings in the stack
    if (_clippingStack.size() > 0)
    {

        for (int i = (int)(_clippingStack.size() - 1); i >= 0; i--)
        {
            auto& clippingShape = _clippingStack[i];
            auto proxyDrawable =
                clippingShape->createProxyDrawable(&clippingShape->clipEnd);
            if (nextDrawable)
            {
                nextDrawable->prev = proxyDrawable;
                proxyDrawable->next = nextDrawable;
            }
            proxyDrawable->prev = nullptr; // End of list
            nextDrawable = proxyDrawable;
        }
    }
    interleaveLayerMasks();
    clearRedundantOperations();
}

// True when `d` belongs inside a bracket whose members are `members`. Pure
// bracket operations (clip and mask markers) are always allowed: they are
// nested structure, not content. A DrawableProxy is allowed when the component
// it stands in for is a member -- a LayoutComponent's proxy is injected by the
// artboard and so never appears in a subtree walk, but its save()+clipPath()
// has to sit inside the same bracket as the restore() in its draw().
static bool spansMember(Drawable* d,
                        const std::unordered_set<Drawable*>& members)
{
    if (members.count(d) != 0)
    {
        return true;
    }
    if (!d->isProxy())
    {
        return false;
    }
    if (d->isClipStart() || d->isClipEnd() || d->isMaskStart() ||
        d->isMaskEnd())
    {
        return true;
    }
    Drawable* stands = d->hittableComponent();
    return stands != nullptr && members.count(stands) != 0;
}

bool Artboard::spliceLayerMaskBracket(LayerMask* mask,
                                      const std::vector<Drawable*>& members,
                                      LayerMaskOp startOp,
                                      LayerMaskOp endOp,
                                      LayerMaskProxyDrawable** startOut)
{
    if (startOut != nullptr)
    {
        *startOut = nullptr;
    }
    if (members.empty())
    {
        return false;
    }
    std::unordered_set<Drawable*> memberSet(members.begin(), members.end());

    // `next` is earlier in draw order, `prev` later: the walk from
    // m_FirstDrawable follows prev, so first/last here are in draw order.
    Drawable* first = nullptr;
    Drawable* last = nullptr;
    for (auto d = m_FirstDrawable; d != nullptr; d = d->prev)
    {
        if (memberSet.count(d) != 0)
        {
            if (first == nullptr)
            {
                first = d;
            }
            last = d;
        }
    }
    if (first == nullptr)
    {
        // The members are not in this artboard's draw list at all (all hidden
        // behind a collapsed parent, say).
        return false;
    }

    // Our markers have to nest with every bracket already in the list, never
    // interleave: the draw walk is handed a [start, end) range and would run
    // off the list if an inner end marker escaped our outer one. The member
    // extremes are not necessarily balanced -- a mask processed before us may
    // have bracketed our first or last member -- so grow the range outward
    // until it is. Bracket markers are always allowed inside a span, so growing
    // can only make the contiguity check below easier.
    bool grew = true;
    while (grew)
    {
        grew = false;
        // Starts whose end is inside the range, and ends whose start is before
        // it.
        int clipDepth = 0, maskDepth = 0;
        int clipDeficit = 0, maskDeficit = 0;
        for (auto d = first;; d = d->prev)
        {
            if (d->isClipStart())
            {
                ++clipDepth;
            }
            else if (d->isClipEnd())
            {
                clipDepth > 0 ? --clipDepth : ++clipDeficit;
            }
            else if (d->isMaskStart())
            {
                ++maskDepth;
            }
            else if (d->isMaskEnd())
            {
                maskDepth > 0 ? --maskDepth : ++maskDeficit;
            }
            if (d == last)
            {
                break;
            }
        }
        // Pull the start back over the openers whose closers we contain.
        while (clipDeficit > 0 || maskDeficit > 0)
        {
            Drawable* before = first->next;
            if (before == nullptr)
            {
                break;
            }
            if (maskDeficit > 0 && before->isMaskStart())
            {
                --maskDeficit;
            }
            else if (clipDeficit > 0 && before->isClipStart())
            {
                --clipDeficit;
            }
            else
            {
                break;
            }
            first = before;
            grew = true;
        }
        // Push the end forward over the closers whose openers we contain.
        while (clipDepth > 0 || maskDepth > 0)
        {
            Drawable* after = last->prev;
            if (after == nullptr)
            {
                break;
            }
            if (maskDepth > 0 && after->isMaskEnd())
            {
                --maskDepth;
            }
            else if (clipDepth > 0 && after->isClipEnd())
            {
                --clipDepth;
            }
            else
            {
                break;
            }
            last = after;
            grew = true;
        }
        if (!grew && (clipDeficit > 0 || maskDeficit > 0 || clipDepth > 0 ||
                      maskDepth > 0))
        {
            // Could not balance: some bracket genuinely half-overlaps this
            // range, and no placement of our markers avoids crossing it.
            return false;
        }
    }

    // Contiguity. A draw rule can lift an unrelated drawable into the middle of
    // a subtree's span; masking it too would be silently wrong, and splitting
    // into two brackets would composite the mask twice. Refuse instead, and let
    // the caller draw the content unmasked.
    for (auto d = first;; d = d->prev)
    {
        if (!spansMember(d, memberSet))
        {
            return false;
        }
        if (d == last)
        {
            break;
        }
    }

    auto* startMarker = mask->createProxyDrawable(startOp);
    auto* endMarker = mask->createProxyDrawable(endOp);

    // Insert startMarker immediately before `first` in draw order.
    Drawable* before = first->next;
    startMarker->next = before;
    startMarker->prev = first;
    if (before != nullptr)
    {
        before->prev = startMarker;
    }
    else
    {
        m_FirstDrawable = startMarker;
    }
    first->next = startMarker;

    // Insert endMarker immediately after `last` in draw order.
    Drawable* after = last->prev;
    endMarker->prev = after;
    endMarker->next = last;
    if (after != nullptr)
    {
        after->next = endMarker;
    }
    last->prev = endMarker;

    startMarker->pairedEnd(endMarker);
    if (startOut != nullptr)
    {
        *startOut = startMarker;
    }
    return true;
}

void Artboard::interleaveLayerMasks()
{
    if (m_layerMasks.empty())
    {
        return;
    }
    for (auto& mask : m_layerMasks)
    {
        mask->resetDrawables();
    }
    for (auto& mask : m_layerMasks)
    {
        // A self-referential mask is inert: its source sits inside the range it
        // would rasterize, so bracketing it would recurse forever.
        if (mask->isSelfReferential())
        {
            continue;
        }
        // Collect the drawables this mask applies to. Done per mask rather than
        // from one shared index because each splice shifts the list.
        std::vector<Drawable*> masked;
        for (auto d = m_FirstDrawable; d != nullptr; d = d->prev)
        {
            const auto& masks = d->layerMasks();
            if (std::find(masks.begin(), masks.end(), mask) != masks.end())
            {
                masked.push_back(d);
            }
        }
        LayerMaskProxyDrawable* start = nullptr;
        if (!spliceLayerMaskBracket(mask,
                                    masked,
                                    LayerMaskOp::maskStart,
                                    LayerMaskOp::maskEnd,
                                    &start))
        {
            continue;
        }
        // Only bracket the source once the masked range is genuinely bracketed:
        // suppressing the source while the content draws unmasked would just
        // make the mask art vanish for nothing.
        LayerMaskProxyDrawable* sourceStart = nullptr;
        if (spliceLayerMaskBracket(mask,
                                   mask->sourceDrawables(),
                                   LayerMaskOp::sourceStart,
                                   LayerMaskOp::sourceEnd,
                                   &sourceStart))
        {
            mask->sourceBracket(sourceStart, sourceStart->pairedEnd());
        }
    }
}

// Look for drawables that are preceeding and succeeding drawables that call
// save and restore. If found, the drawable does not need to call save and
// restore itself.
void Artboard::clearRedundantOperations()
{
    Drawable* currentDrawable = m_FirstDrawable;
    bool prevAppliedSave = false;
    // Keep a stack of clipStart operation results to apply the same operation
    // to its clipEnd
    std::vector<bool> appliedClippingSaveOperations;
    while (currentDrawable)
    {
        currentDrawable->needsSaveOperation(true);
        // If previous operation applied a save operation
        if (prevAppliedSave)
        {
            // With consecutive clippings, we can skip the save and restore
            // operation since the previous one has applied it
            if (currentDrawable->isClipStart())
            {
                appliedClippingSaveOperations.push_back(false);
                currentDrawable->needsSaveOperation(false);
            }
            else if (currentDrawable->isClipEnd())
            {
                // Apply or skip the clipEnd Restore operation matching its clip
                // start counterpart
                auto operationApplied = appliedClippingSaveOperations.back();
                appliedClippingSaveOperations.pop_back();
                currentDrawable->needsSaveOperation(operationApplied);
            }
            else
            {
                // Check if next is clip end, if it is, we can skip the drawable
                // save/restore because it is tightly wrapped in a clipping
                // operation
                auto nextDrawable = currentDrawable->prev;
                if (nextDrawable->isClipEnd())
                {
                    currentDrawable->needsSaveOperation(false);
                }
            }
        }
        else if (currentDrawable->isClipStart())
        {
            appliedClippingSaveOperations.push_back(true);
        }
        else if (currentDrawable->isClipEnd())
        {
            // Apply or skip the clipEnd Restore operation matching its clip
            // start counterpart
            auto operationApplied = appliedClippingSaveOperations.back();
            currentDrawable->needsSaveOperation(operationApplied);
            appliedClippingSaveOperations.pop_back();
        }
        prevAppliedSave = currentDrawable->isClipStart() &&
                          (currentDrawable->willClip() || prevAppliedSave);
        currentDrawable = currentDrawable->prev;
    }
    assert(appliedClippingSaveOperations.size() == 0);
}

#ifdef WITH_RIVE_EDITOR
void Artboard::initLayoutForEditor()
{
    m_layout = Layout(0.0f, 0.0f, width(), height());
    // NOTE: intentionally skipping `markLayoutDirty(this)` — the full
    // layout pipeline isn't wired for coop-loaded artboards yet (no
    // style / Yoga node setup), and running syncStyleChanges on a
    // partial state has been observed to crash. Phase 2.1 adds the
    // layout pipeline back behind the `#ifdef WITH_RIVE_LAYOUT` once
    // the missing setup is in place.
}
#endif

Component* Artboard::componentForOrderEntry(
    const DependencyOrderEntry& entry) const
{
    if (entry.objectIndex >= m_Objects.size())
    {
        return nullptr;
    }
    Core* object = m_Objects[entry.objectIndex];
    if (object == nullptr)
    {
        return nullptr;
    }
    if (entry.helperSlot == 0)
    {
        return object->is<Component>() ? object->as<Component>() : nullptr;
    }
    // Slot 1 is the single owned helper for the types that have one. Add
    // further slots here if another Component gains an owned graph node.
    if (entry.helperSlot == 1)
    {
        if (object->is<Shape>())
        {
            return object->as<Shape>()->pathComposer();
        }
        if (object->is<TextStyle>())
        {
            return object->as<TextStyle>()->variationHelper();
        }
    }
    return nullptr;
}

const std::vector<Artboard::DependencyOrderEntry>* Artboard::
    dependencyOrderRecipe() const
{
    switch (m_RecipeState)
    {
        case RecipeState::valid:
            return &m_DependencyOrderRecipe;
        case RecipeState::unusable:
            return nullptr;
        case RecipeState::unbuilt:
            break;
    }

    // No reverse Component* -> index map is needed: sortDependencies() has
    // already stamped every component in m_DependencyOrder with its position
    // via m_GraphOrder, so one walk of m_Objects can scatter each entry
    // straight into its slot. That keeps recipe construction allocation-free
    // beyond the result vector, which matters because the source artboard is
    // re-decoded (and so re-builds this) on every editor regeneration.
    const size_t count = m_DependencyOrder.size();
    constexpr uint32_t unset = ~(uint32_t)0;
    m_DependencyOrderRecipe.assign(count, {unset, 0});
    size_t filled = 0;

    auto stamp = [&](const Component* component, uint32_t index, uint8_t slot) {
        if (component == nullptr)
        {
            return;
        }
        const unsigned int position = component->graphOrder();
        // A component the sort never placed keeps Component's sentinel, which
        // no order can hand out. One that a *previous, longer* order placed
        // keeps that order's position, which this one may well have handed to
        // somebody else -- so the stamp alone is not proof. Confirm the order
        // really holds this component where it claims to be, which is an exact
        // membership test no stale stamp can pass.
        if (position >= count || m_DependencyOrder[position] != component)
        {
            return;
        }
        DependencyOrderEntry& entry = m_DependencyOrderRecipe[position];
        if (entry.objectIndex != unset)
        {
            // One component reachable through two slots. Leave the first.
            return;
        }
        entry = {index, slot};
        filled++;
    };

    for (size_t i = 0; i < m_Objects.size(); i++)
    {
        Core* object = m_Objects[i];
        if (object == nullptr || !object->is<Component>())
        {
            continue;
        }
        const uint32_t index = (uint32_t)i;
        stamp(object->as<Component>(), index, 0);
        if (object->is<Shape>())
        {
            stamp(object->as<Shape>()->pathComposer(), index, 1);
        }
        else if (object->is<TextStyle>())
        {
            stamp(object->as<TextStyle>()->variationHelper(), index, 1);
        }
    }

    if (filled != count)
    {
        // Something in the order isn't addressable as index/slot (or the
        // graph-order stamps disagree). Don't emit a partial order.
        m_DependencyOrderRecipe.clear();
        m_RecipeState = RecipeState::unusable;
        return nullptr;
    }
    m_RecipeState = RecipeState::valid;
    return &m_DependencyOrderRecipe;
}

bool Artboard::replaySourceDependencyOrder(const Artboard* source)
{
    if (source == nullptr || source == this ||
        source->m_Objects.size() != m_Objects.size())
    {
        return false;
    }
    const std::vector<DependencyOrderEntry>* recipe =
        source->dependencyOrderRecipe();
    if (recipe == nullptr)
    {
        return false;
    }
    std::vector<Component*> order;
    order.reserve(recipe->size());
    for (const DependencyOrderEntry& entry : *recipe)
    {
        Component* component = componentForOrderEntry(entry);
        if (component == nullptr)
        {
            // A helper the source had but this instance doesn't (they are
            // lazily created). Fall back rather than emit a short order.
            return false;
        }
        order.push_back(component);
    }
    m_DependencyOrder = std::move(order);
    return true;
}

void Artboard::sortDependencies()
{
    // An instance replays its source's order instead of re-walking the graph;
    // see DependencyOrderEntry. Falls back to sorting whenever anything about
    // the instance doesn't line up.
    bool replayed = replaySourceDependencyOrder(m_artboardSource);

    if (!replayed)
    {
        DependencySorter sorter;
        sorter.sort(this, m_DependencyOrder);
    }
    unsigned int graphOrder = 0;
    for (auto component : m_DependencyOrder)
    {
        component->m_GraphOrder = graphOrder++;
    }
    m_DependencyOrderRecipe.clear();
    m_RecipeState = RecipeState::unbuilt;
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

void Artboard::initScriptedObjects()
{
    if (isInstance())
    {
        for (auto obj : m_ScriptedObjects)
        {
            if (obj->scriptAsset() != nullptr)
            {
                if (!obj->userLuaInitDone())
                {
                    obj->scriptAsset()->initScriptedObject(obj);
                }
                obj->hydrateScriptInputs();
            }
        }
    }
}

void Artboard::pollAsyncWork()
{
    rive_pollAsyncWork();
#ifdef WITH_RIVE_SCRIPTING_WASM
    if (auto f = artboardFile())
    {
        for (auto& vm : f->wasmVMs())
        {
            vm->deliverHeldOutcomes();
        }
    }
#endif
}

void Artboard::advanceScriptedViewModels()
{
#ifdef WITH_RIVE_SCRIPTING_LUAU
    if (m_scriptingVM != nullptr)
    {
        if (auto* context = m_scriptingVM->context())
        {
            context->advanceDetachedViewModels();
        }
    }
#endif
#ifdef WITH_RIVE_SCRIPTING_WASM
    if (auto f = artboardFile())
    {
        for (auto& vm : f->wasmVMs())
        {
            vm->advanceDetachedViewModels();
        }
    }
#endif
}

Core* Artboard::resolve(Id id) const
{
#ifdef WITH_RIVE_EDITOR
    // Editor `Id` is `{client, object}` and objects come from two
    // sources that share the same namespace:
    //   • `.riv`-loaded objects — `CoreIdType::runtimeDeserialize`
    //     synthesizes `Id{0, runtime_index}` which indexes straight
    //     into `m_Objects` (same behavior as a runtime build).
    //   • Coop-delivered objects — stamped by the server with
    //     `client == 0` too, looked up via `m_editorResolver`'s
    //     CoopId map. Non-zero client always means a coop peer ID.
    // `m_Objects` first (runtime path) then fall through to the
    // resolver, so both sources resolve correctly without the caller
    // needing to know which. `kEmptyId == {0, 0}` is short-circuited
    // — `m_Objects[0]` is a legitimate object (the Artboard itself)
    // and would be a wrong answer for "no id set".
    if (id.empty())
        return nullptr;
    if (id.client == 0 && id.object < m_Objects.size())
    {
        if (auto* hit = m_Objects[id.object])
            return hit;
    }
    return m_editorResolver ? m_editorResolver->resolve(id) : nullptr;
#else
    // Runtime: single-source `Id` (a raw uint index) into `m_Objects`.
    return id < static_cast<int>(m_Objects.size()) ? m_Objects[id] : nullptr;
#endif
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
#ifdef TESTING
    sm_dirtNotifications++;
#endif
    wakeIfQuietRow();
    m_didChange = true;
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
#ifdef TESTING
    sm_dirtNotifications++;
#endif
    wakeIfQuietRow();
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
            artboard->addDataBind(dataBind->cloneWithTarget(clone));
        }
    }
}

void Artboard::syncInstanceValueBinds()
{
    // Mid update the removes and adds would only queue, and a second change
    // in the same pass could not see them; coalesce until the pass drains.
    if (isProcessingDataBinds())
    {
        m_instanceValueBindsPending = true;
        return;
    }
    auto instance = dataBindContext() != nullptr
                        ? dataBindContext()->mainViewModelInstance()
                        : nullptr;
    if (instance == m_instanceValueBindsSource)
    {
        return;
    }
    // Snapshot: removing mutates the list we are walking.
    auto previous = dataBinds();
    for (auto dataBind : previous)
    {
        if (dataBind->isInstanceValueBind())
        {
            removeAndDeleteDataBind(dataBind);
        }
    }
    m_instanceValueBindsSource = instance;
    if (instance == nullptr)
    {
        return;
    }
    for (auto dataBind : instance->valueDataBinds())
    {
        auto dataBindClone = dataBind->cloneWithTarget(dataBind->target());
        dataBindClone->markInstanceValueBind();
        addDataBind(dataBindClone);
    }
}

void Artboard::mainViewModelInstanceChanged()
{
    wakeIfQuietRow();
    syncInstanceValueBinds();
}

void Artboard::dataContextChanged() { wakeIfQuietRow(); }

void Artboard::wakeQuietRow()
{
    auto row = m_quietHostRow;
    m_quietHostRow = kNoQuietRow;
    if (m_host != nullptr)
    {
        m_host->hostedRowWoke(this, row);
    }
}

AdvancingComponent::QuietState Artboard::rowQuietState()
{
    using QuietState = AdvancingComponent::QuietState;
    // Work this check can't see into: hosted artboards and lists, joysticks,
    // resettables and scripts.
    if (!m_ArtboardHosts.empty() || !m_Joysticks.empty() ||
        !m_Resettables.empty() || !m_ScriptedObjects.empty())
    {
        return QuietState::never;
    }
    // Cheapest first: a busy row is checked again every frame.
    if (hasDirt(ComponentDirt::Components) || !m_dirtyLayout.empty() ||
        m_hostTransformMarkedDirty || m_instanceValueBindsPending ||
        hasDataBindWork())
    {
        return QuietState::busy;
    }
    auto state = QuietState::quiet;
    for (auto advancing : m_advancingComponents)
    {
        switch (advancing->quietState())
        {
            case QuietState::never:
                return QuietState::never;
            case QuietState::busy:
                state = QuietState::busy;
                break;
            case QuietState::quiet:
                break;
        }
    }
    if (state == QuietState::quiet && mayAdvanceDataBinds())
    {
        // Converters that advance can't say whether they would.
        return QuietState::never;
    }
    return state;
}

void Artboard::dataBindsProcessed()
{
    if (m_instanceValueBindsPending)
    {
        m_instanceValueBindsPending = false;
        syncInstanceValueBinds();
    }
}

void Artboard::buildKeyFrameSourceBindsIndex() const
{
    m_keyFrameSourceBindsBuilt = true;
    for (auto dataBind : dataBinds())
    {
        auto target = dataBind->target();
        if (target == nullptr || !target->is<KeyFrame>())
        {
            continue;
        }
        // A keyframe holds a single value, so keep the first bind per target
        // (emplace does not overwrite an existing key).
        m_keyFrameSourceBinds.emplace(target->as<KeyFrame>(), dataBind);
    }
}

bool Artboard::hasKeyFrameSourceBinds() const
{
    if (!m_keyFrameSourceBindsBuilt)
    {
        buildKeyFrameSourceBindsIndex();
    }
    return !m_keyFrameSourceBinds.empty();
}

DataBind* Artboard::keyFrameSourceBind(const KeyFrame* keyframe) const
{
    if (!m_keyFrameSourceBindsBuilt)
    {
        buildKeyFrameSourceBindsIndex();
    }
    auto it = m_keyFrameSourceBinds.find(keyframe);
    return it != m_keyFrameSourceBinds.end() ? it->second : nullptr;
}

void Artboard::host(ArtboardHost* artboardHost)
{
    addedToHost();
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
    auto& renderPaths = mutableRenderPaths();
    renderPaths.local.rewind();
    renderPaths.local.addRect(bg);
    renderPaths.world.rewind();
    renderPaths.world.addRect(clip);
}

void Artboard::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::DrawOrder))
    {
        sortDrawOrder();
    }
    if (hasDirt(value, ComponentDirt::Clipping))
    {
        clearRedundantOperations();
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
    wakeIfQuietRow();
    // Most artboard data binds target Components and need the component graph
    // marked dirty. Keyframe value binds instead target transient
    // BindableProperty holders (see
    // LinearAnimationInstance::keyFrameValueHolder) that aren't in the
    // component graph — they're read directly during animation apply — so only
    // propagate component dirt for Component targets.
    auto target = dataBind->target();
    if (target != nullptr && target->is<Component>())
    {
        onComponentDirty(target->as<Component>());
    }
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
    wakeIfQuietRow();
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
    wakeIfQuietRow();
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
    // Always pass NAN and let calculateLayoutInternal decide whether to use
    // the intrinsic (hug) size or fall back to width()/height().
    //
    // This covers all cases:
    // - Runtime:
    //   - Top level artboards: intrinsically sized artboards get NAN so Yoga
    //     computes from children; fixed-size artboards fall back to
    //     width()/height() inside calculateLayoutInternal.
    //   - Nested node/leaf artboards (m_updatesOwnLayout == true): same as
    //     top-level, calculateLayoutInternal handles the distinction.
    //   - Nested layout-mode artboards (NestedArtboardLayout): their layout
    //     node is owned by the parent via takeLayoutData(), which sets
    //     m_updatesOwnLayout = false. syncStyleChangesWithUpdate() gates on
    //     that flag, so calculateLayout() is not reached for these.
    // - Editor:
    //   - Top level artboards: are handled on the Dart side so don't take
    //     this code path
    //   - Nested node/leaf artboards (m_updatesOwnLayout == true):
    //     same as runtime, calculateLayoutInternal handles the distinction.
    //   - Nested layout-mode artboards (NestedArtboardLayout): same
    //     as runtime, their layout node is owned by the Dart parent
    //     which sets m_updatesOwnLayout = false
#ifdef TESTING
    sm_layoutPassCount++;
#endif
    calculateLayoutInternal(NAN, NAN);
}

bool Artboard::updatePass(bool isRoot)
{
    RIVE_PROF_SCOPE()
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
    RIVE_PROF_SCOPE()
    bool didUpdate = false;

    for (auto adv : m_advancingComponents)
    {
        if (adv->advanceComponent(elapsedSeconds, flags))
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
    if (m_Resettables.size() == 0)
    {
        return;
    }
    for (auto obj : m_Resettables)
    {
        obj->reset();
    }
}

bool Artboard::advance(float elapsedSeconds, AdvanceFlags flags)
{
    // Poll async work (image decodes, etc.) so promises resolve before
    // script advance() callbacks run.
    pollAsyncWork();

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
    // Mirror drawInternal's own rotation/scale so hit-testing matches what is
    // drawn. This single spot also covers nested instances, since
    // NestedArtboard::hitTest re-enters Artboard::hitTest.
    if (hasSelfTransform())
    {
        mx *= selfTransform();
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
    // When this artboard is nested, its own rotation/scale (applied about the
    // origin at draw time) is part of how its contents land in the parent, so
    // fold it in before mapping through the host. Top-level artboards are the
    // root coordinate space, so their own transform is not applied here.
    if (host())
    {
        auto local = hasSelfTransform() ? selfTransform() * point : point;
        return host()->hostTransformPoint(local, this->as<ArtboardInstance>());
    }
#ifdef WITH_RIVE_TOOLS
    // Editor artboards don't have a host, so we expose a function that calls
    // the host in dart. The callback is only wired up for mounted (nested)
    // instances, so applying the self transform here matches the host() path.
    if (m_rootTransformCallback != nullptr)
    {
        auto local = hasSelfTransform() ? selfTransform() * point : point;
        auto x =
            m_rootTransformCallback(callbackUserData, local.x, local.y, true);
        auto y =
            m_rootTransformCallback(callbackUserData, local.x, local.y, false);
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

void Artboard::watermark(std::unique_ptr<Watermark> watermark)
{
    m_watermark = std::move(watermark);
}

bool Artboard::advanceWatermark(float elapsedSeconds)
{
    if (m_watermark == nullptr)
    {
        return false;
    }
    if (!m_watermark->advance(elapsedSeconds))
    {
        m_watermark = nullptr;
        return false;
    }
    return true;
}

void Artboard::draw(Renderer* renderer)
{
    sm_frameId++;
    // Nested artboards and component lists draw through drawInternal, so only a
    // top level draw can be diverted to the watermark. isPlaying() keeps an
    // artboard that is never advanced through a state machine (and so never
    // starts its watermark) drawing itself rather than freezing on a pre-roll
    // that would never end.
    if (m_watermark != nullptr && m_watermark->isPlaying())
    {
        m_watermark->draw(renderer, bounds());
        return;
    }
    // A standalone/root artboard is never cached as a bitmap: it is already the
    // top-level render target, and a host can skip drawing entirely via
    // didChange(). Only nested/instanced draws (which reach drawInternal
    // directly) participate in cache-as-bitmap.
    drawContent(renderer);
}

struct ModulatedDraw
{
    uint32_t propertyKey;
    // Alpha, red, green, blue of the tags above, multiplied in float so
    // depth does not round.
    std::array<float, 4> level = {1.0f, 1.0f, 1.0f, 1.0f};
};

static void modulatedDrawVisitor(void* context,
                                 Drawable* drawable,
                                 Renderer* renderer)
{
    auto modulated = static_cast<ModulatedDraw*>(context);
    auto property = drawable->customProperty(modulated->propertyKey);
    std::array<float, 4> own;
    if (property != nullptr && property->is<CustomPropertyNumber>())
    {
        float value =
            math::clamp(property->as<CustomPropertyNumber>()->propertyValue(),
                        0.0f,
                        1.0f);
        own = {1.0f, value, value, value};
    }
    else if (property != nullptr && property->is<CustomPropertyColor>())
    {
        ColorInt color = property->as<CustomPropertyColor>()->propertyValue();
        own = {colorOpacity(color),
               colorRed(color) / 255.0f,
               colorGreen(color) / 255.0f,
               colorBlue(color) / 255.0f};
    }
    else
    {
        // Tagged, but not with this key.
        drawable->draw(renderer);
        return;
    }
    std::array<float, 4> outer = modulated->level;
    unsigned int channels[4];
    for (size_t i = 0; i < own.size(); i++)
    {
        modulated->level[i] = outer[i] * own[i];
        channels[i] = (unsigned int)std::lround(modulated->level[i] * 255.0f);
    }
    renderer->modulateColor(
        colorARGB(channels[0], channels[1], channels[2], channels[3]),
        true);
    drawable->draw(renderer);
    modulated->level = outer;
}

void Artboard::drawModulated(Renderer* renderer,
                             uint32_t propertyKey,
                             const File* keysFile)
{
    ModulatedDraw modulated = {propertyKey};
    drawInternal(renderer, modulatedDrawVisitor, &modulated, keysFile);
}

const File* Artboard::drawVisitorFile() const
{
    return m_drawVisitorFile != nullptr ? m_drawVisitorFile
                                        : artboardFile().get();
}

void Artboard::drawHosted(Artboard* hosted, Renderer* renderer)
{
    DrawVisitor visitor = m_drawVisitor;
    if (visitor != nullptr)
    {
        // Name ids are per file, so a key means nothing in an artboard bound
        // in from another one.
        const File* file = drawVisitorFile();
        const File* hostedFile = hosted->artboardFile().get();
        if (hostedFile != nullptr && hostedFile != file)
        {
            visitor = nullptr;
        }
        hosted->m_drawVisitorFile = file;
    }
    hosted->drawInternal(renderer, visitor, m_drawVisitorContext);
}

void Artboard::drawInternal(Renderer* renderer,
                            DrawVisitor visitor,
                            void* visitorContext,
                            const File* keysFile)
{
    if (keysFile != nullptr)
    {
        m_drawVisitorFile = keysFile;
    }
#ifdef RIVE_CANVAS
    // A cached bitmap has no drawables left to visit.
    if (visitor == nullptr && m_BitmapCache != nullptr &&
        drawCachedAsBitmap(renderer))
    {
        return;
    }
#endif
    drawContent(renderer, visitor, visitorContext);
}

void Artboard::drawContent(Renderer* renderer,
                           DrawVisitor visitor,
                           void* visitorContext)
{
    RIVE_PROF_SCOPE_L(1)
    m_didChange = false;
    if (childOpacity() == 0)
    {
        return;
    }
    // Hosted artboards read these while this draw runs. A draw started from
    // inside a visit hands them back, and none outlives its context.
    struct VisitorScope
    {
        Artboard* artboard;
        DrawVisitor visitor;
        void* context;
        ~VisitorScope()
        {
            artboard->m_drawVisitor = visitor;
            artboard->m_drawVisitorContext = context;
        }
    } visitorScope{this, m_drawVisitor, m_drawVisitorContext};
    m_drawVisitor = visitor;
    m_drawVisitorContext = visitorContext;
    bool hasSelf = hasSelfTransform();
    bool save = clip() || m_FrameOrigin || hasSelf;
    if (save)
    {
        renderer->save();
    }

    if (m_FrameOrigin)
    {
        Mat2D artboardTransform;
        artboardTransform[4] = layoutWidth() * originX();
        artboardTransform[5] = layoutHeight() * originY();
        renderer->transform(artboardTransform);
    }

    // Apply the artboard's own rotation/scale, pivoted around its origin.
    // Content-local (0,0) is the origin anchor, so this is simply rotation *
    // scale with no extra pivot translation. Applied after the frame-origin
    // translation so the anchor stays put, and it covers both top-level and
    // nested (mounted) rendering since both funnel through drawInternal.
    // Hit-testing mirrors this via selfTransform() so interaction matches.
    if (hasSelf)
    {
        renderer->transform(selfTransform());
    }

    // Clip after the frame-origin and self transforms so the clip region tracks
    // the (possibly rotated/scaled) artboard. The local path is the
    // content-local bounds, so it is transformed along with the content.
    if (clip())
    {
        renderer->clipPath(mutableRenderPaths().local.renderPath(this));
    }

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
    drawDrawableRange(renderer, m_FirstDrawable, nullptr);
    if (save)
    {
        renderer->restore();
    }
}

void Artboard::drawDrawableRange(Renderer* renderer,
                                 Drawable* first,
                                 Drawable* stop)
{
    // Empty clips is a counter for clipping shapes that are empty, for
    // example because they are hidden in a solo. If emptyClips > 0, the
    // drawables should not be drawn. A layer mask's source bracket rides the
    // same counter to suppress the source in the normal pass.
    int emptyClips = 0;
    // We stack clip operations to avoid calling a save + clip + restore on
    // clipping that don't have any drawables in between. this is a common
    // case with drawables in solos where the drawables are not drawn.
    // Deliberately a local: this function recurses through drawMasked.
    std::vector<Drawable*> pendingClipOperations;
    for (auto drawable = first; drawable != stop; drawable = drawable->prev)
    {
        auto prevClips = emptyClips;
        emptyClips += drawable->emptyClipCount();
        if (!drawable->willDraw() || emptyClips != prevClips || emptyClips > 0)
        {
            continue;
        }
        if (drawable->isClipStart())
        {
            pendingClipOperations.push_back(drawable);
            continue;
        }
        else if (pendingClipOperations.size() > 0)
        {
            // If there are clip operations pending and the next drawable is
            // a clip end, the clipping operation does not clip anything and
            // both can be skipped.
            if (drawable->isClipEnd())
            {
                pendingClipOperations.pop_back();
                continue;
            }
            else
            {
                for (auto& pendingClip : pendingClipOperations)
                {
                    pendingClip->draw(renderer);
                }
                pendingClipOperations.clear();
            }
        }
        if (drawable->isMaskStart())
        {
            // Pending clips have already been flushed above, so the ancestor
            // clips are live on `renderer` and will constrain the composite.
            auto* startMarker = static_cast<LayerMaskProxyDrawable*>(drawable);
            Drawable* end = startMarker->pairedEnd();
            if (drawMasked(renderer, startMarker, end))
            {
                // Consumed: skip to the closing marker. `continue` runs the
                // for-increment, so the next iteration starts at end->prev.
                drawable = end;
            }
            // Otherwise fall through and let the range draw inline; the marker
            // itself draws nothing either way.
            continue;
        }
        if (drawable->isMaskEnd())
        {
            continue;
        }
        if (m_drawVisitor != nullptr && drawable->hasCustomProperties())
        {
            // Whatever the visitor sets on the renderer ends with the visit.
            renderer->save();
            m_drawVisitor(m_drawVisitorContext, drawable, renderer);
            renderer->restore();
        }
        else
        {
            drawable->draw(renderer);
        }
    }
}

#ifdef RIVE_CANVAS
namespace
{
// How much to pad a drawable that reported `approximate` bounds. Generous on
// purpose: the mask box is intersected with the artboard's own bounds, so the
// worst case of an over-wide margin is a raster no bigger than the one this
// whole change replaces -- while the worst case of an under-wide one is a hard
// crop through the middle of someone's artwork.
constexpr float kApproximateSlopDevicePx = 8.0f;
constexpr float kApproximateSlopFraction = 0.25f;

AABB padApproximate(const AABB& box, float rasterScale)
{
    const float relative =
        kApproximateSlopFraction * std::max(box.width(), box.height());
    const float absolute = rasterScale > 0.0f
                               ? kApproximateSlopDevicePx / rasterScale
                               : kApproximateSlopDevicePx;
    const float slop = std::max(relative, absolute);
    if (!std::isfinite(slop) || slop <= 0.0f)
    {
        return box;
    }
    return box.outset(slop, slop);
}

// Float AABB has no intersect helper, and AABB::overlaps is wrong (it compares
// maxX against b.minY), so this is written out. False for an empty result.
bool intersectBoxes(const AABB& a, const AABB& b, AABB* out)
{
    const AABB r(std::max(a.left(), b.left()),
                 std::max(a.top(), b.top()),
                 std::min(a.right(), b.right()),
                 std::min(a.bottom(), b.bottom()));
    if (r.isEmptyOrNaN())
    {
        return false;
    }
    *out = r;
    return true;
}
} // namespace

BoundsFidelity Artboard::rangeDrawBounds(Drawable* first,
                                         Drawable* stop,
                                         float rasterScale,
                                         AABB* out,
                                         bool* anyDrawn) const
{
    // The skip rules mirror drawDrawableRange deliberately, which is what buys
    // the awkward cases for free: a drawable inside a nested empty clip is
    // excluded, and so is a nested mask's source range -- its markers push the
    // same empty-clip counter -- while a nested mask's own members are
    // included, which is correct because a mask only ever attenuates what is
    // already there.
    int emptyClips = 0;
    AABB accumulated = AABB::forExpansion();
    BoundsFidelity worst = BoundsFidelity::exact;
    bool drew = false;

    for (auto* d = first; d != stop; d = d->prevDrawable())
    {
        const int prevClips = emptyClips;
        emptyClips += d->emptyClipCount();
        if (!d->willDraw() || emptyClips != prevClips || emptyClips > 0)
        {
            continue;
        }
        // Bracket markers paint nothing of their own: a clip only constrains
        // what follows it, and a mask marker is consumed by drawMasked.
        if (d->isClipStart() || d->isClipEnd() || d->isMaskStart() ||
            d->isMaskEnd())
        {
            continue;
        }
        drew = true;

        AABB painted;
        const BoundsFidelity fidelity = d->paintedWorldBounds(&painted);
        if (fidelity == BoundsFidelity::none)
        {
            // Not "pad it harder": there is no measurement to pad. Give up on a
            // tight box for this range rather than inventing one.
            *anyDrawn = true;
            return BoundsFidelity::none;
        }
        if (fidelity == BoundsFidelity::approximate)
        {
            worst = BoundsFidelity::approximate;
            painted = padApproximate(painted, rasterScale);
        }
        // A drawable can legitimately paint nothing -- a shape whose paths all
        // collapsed reports an empty box. Filtering here also keeps
        // AABB::forExpansion()'s +/-FLT_MAX sentinel out of the union.
        if (!painted.isEmptyOrNaN())
        {
            accumulated.expand(painted);
        }
    }

    *anyDrawn = drew;
    if (!drew)
    {
        return BoundsFidelity::exact;
    }
    if (accumulated.isEmptyOrNaN())
    {
        // Something drew but nothing measurable came of it. Rare, and not worth
        // reasoning about further: fall back.
        return BoundsFidelity::none;
    }
    *out = accumulated;
    return worst;
}

bool Artboard::drawMasked(Renderer* renderer,
                          Drawable* startMarker,
                          Drawable* endMarker)
{
    LayerMask& mask =
        *static_cast<LayerMaskProxyDrawable*>(startMarker)->mask();
    if (!mask.isVisible())
    {
        return false;
    }

    // Re-entry guard, and defence in depth rather than a fix for anything
    // reachable today.
    //
    // onAddedClean rejects a mask whose source sits under its own parent, which
    // only catches a mask pointing at itself. Two masks naming each other's
    // subtrees pass that test, and the coverage pass draws a RANGE that
    // drawDrawableRange dispatches from, so in principle A could rasterize B
    // which rasterizes A without bound.
    //
    // In practice spliceLayerMaskBracket grows every range outward until it is
    // balanced with the brackets already in the list, so brackets nest and
    // never interleave: if B's members sit inside A's source bracket, B's range
    // grows to enclose the whole of A, which puts A's source range INSIDE B
    // rather than the other way round, and walking it never reaches B's start
    // marker. A mutual pair therefore terminates today -- there is a
    // [layer-mask] test pinning exactly that -- and this costs a bool and a
    // branch against the day the nesting invariant is relaxed, or a longer
    // cycle is reached some other way.
    //
    // Per mask, so legitimate nesting -- a different LayerMask inside a masked
    // subtree -- is unaffected.
    if (mask.m_isDrawing)
    {
        return false;
    }
    // Cleared however this returns, and there are many exits below.
    struct DrawingGuard
    {
        LayerMask& mask;
        DrawingGuard(LayerMask& m) : mask(m) { mask.m_isDrawing = true; }
        ~DrawingGuard() { mask.m_isDrawing = false; }
    } drawingGuard(mask);

    // canvasContentHost, not deferredCanvasHost: this only needs somewhere to
    // rasterize into. No host means nobody can give us an offscreen frame (a
    // plain non-GPU or test factory), so the content draws unmasked -- the
    // permanent fallback, and the reason a mask degrades rather than vanishing.
    Factory* f = factory();
    auto* host = f ? f->canvasContentHost() : nullptr;
    if (host == nullptr)
    {
        return false;
    }

    // The same fallback, one step earlier, for a host that can rasterize but
    // cannot apply the mask. Renderer::applyLayerMask already no-ops there, but
    // finding out that late is too late: the rasters below are sized to the
    // content-and-coverage intersection, so the layer would come back unmasked
    // and *cropped* to a box that only made sense if the mask had been applied.
    // Bailing here draws the range inline -- unmasked for real, and two
    // canvases and a composite cheaper.
    if (!host->supportsLayerMask())
    {
        return false;
    }

    // The raster scale before anything is measured: it depends only on the
    // renderer's transform, and it is the unit the slop margin for an
    // approximately-bounded drawable is expressed against.
    offscreen::RasterPlan plan;
    if (!offscreen::planRasterScale(renderer, mask.resolution(), &plan))
    {
        return false;
    }

    // mask.m_dirty alone. LayerMask sits on the dependency graph as a dependent
    // of everything in its own range and its own source, so its update() raises
    // this when -- and only when -- something it actually rasterizes changed.
    //
    // This used to also read the artboard-wide "something moved" flag, which
    // meant every mask re-rasterized both of its canvases whenever anything
    // anywhere in the artboard moved, however unrelated. That flag was captured
    // into a separate field just for this read, and both are now gone.
    const bool contentChanged = mask.m_contentCanvas == nullptr ||
                                mask.m_maskCanvas == nullptr || mask.m_dirty;

    AABB tight;
    if (contentChanged)
    {
        // Measuring both ranges also answers "would anything draw at all",
        // which is what the two cheap probes that used to live here did on
        // their own.
        bool contentDraws = false;
        AABB contentBox;
        const BoundsFidelity contentFidelity =
            rangeDrawBounds(startMarker->prevDrawable(),
                            endMarker,
                            plan.rasterScale,
                            &contentBox,
                            &contentDraws);
        if (!contentDraws)
        {
            // Nothing in the range would have drawn, so consume the bracket
            // rather than rasterizing two empty textures.
            return true;
        }

        bool coverageDraws = false;
        AABB sourceBox;
        BoundsFidelity sourceFidelity = BoundsFidelity::none;
        if (mask.sourceStart() == nullptr)
        {
            // No source bracket. Two very different situations arrive here and
            // the mode-switch below is only right for one of them.
            //
            // interleaveLayerMasks splices the masked range first and the
            // source second, and the second can fail on its own -- a draw rule
            // can scatter the source subtree so it is not contiguous. The
            // masked bracket is already in the list by then, so this mask is
            // live with no source bracket, and the source subtree is NOT
            // suppressed: it draws in its natural position. Treating coverage
            // as absent would erase the whole masked range in alpha/luminance
            // while the mask art itself stayed on screen -- the content gone
            // and the mask visible, which is the worst of both.
            //
            // So only take the empty-coverage path when the source genuinely
            // draws nothing. If anything in it would draw, the bracket is the
            // thing that is missing, and the mask goes inert: return false and
            // the range draws inline, unmasked.
            for (Drawable* d : mask.sourceDrawables())
            {
                if (d->willDraw())
                {
                    return false;
                }
            }
        }
        else
        {
            // Over the source bracket rather than sourceDrawables(), matching
            // what the coverage pass below actually draws -- so a clipping
            // shape nested inside the source is accounted for the same way.
            sourceFidelity = rangeDrawBounds(mask.sourceStart()->prevDrawable(),
                                             mask.sourceEnd(),
                                             plan.rasterScale,
                                             &sourceBox,
                                             &coverageDraws);
        }
        if (!coverageDraws)
        {
            // Coverage is zero everywhere -- the mask source is hidden, or sits
            // in an inactive solo. Which way that falls depends on the mode,
            // and both answers avoid the GPU entirely.
            switch (mask.maskMode())
            {
                case MaskMode::alpha:
                case MaskMode::luminance:
                    return true; // content fully masked out
                case MaskMode::invertedAlpha:
                case MaskMode::invertedLuminance:
                    return false; // content fully visible; draw it inline
            }
        }

        // One box and one scale for both canvases, so a content texel and a
        // coverage texel at the same coordinate correspond with no resampling
        // and no second transform.
        bool authoredBox = false;
        const AABB authored = mask.customBounds();
        if (mask.useCustomBounds() && !authored.isEmptyOrNaN())
        {
            // The author drew this rectangle, in artboard space. Taken
            // verbatim: no slop margin, and deliberately no intersect with the
            // artboard box either, because cropping a mask to a region that
            // reaches outside the artboard is intent rather than an error.
            // kMaxDim still caps it.
            //
            // The ranges above were still walked, which is what keeps the
            // "nothing draws" and "no coverage" shortcuts working; only the box
            // comes from here. The mode-aware intersect is skipped on purpose
            // -- the author has already said which region matters.
            //
            // A degenerate rectangle -- zero or negative extent, which is what
            // a half-drawn one looks like -- falls through to measuring rather
            // than collapsing the raster and dropping the layer.
            tight = authored;
            authoredBox = true;
        }
        else if (contentFidelity == BoundsFidelity::none)
        {
            // Something in the range could not say where it paints. Fall back
            // to the box that is always big enough.
            tight = bounds();
        }
        else
        {
            tight = contentBox;
            // In the un-inverted modes the coverage texel is zero outside the
            // source, so content out there is erased anyway: intersecting costs
            // nothing and it is where the win lives, because a small mask over
            // a full-stage layer now rasterizes only the small mask. The
            // inverted modes are the opposite -- absent coverage means a factor
            // of 1, so the content box has to stand on its own.
            const bool intersectsSource =
                mask.maskMode() == MaskMode::alpha ||
                mask.maskMode() == MaskMode::luminance;
            if (sourceFidelity != BoundsFidelity::none)
            {
                // sourceBox is a superset of where coverage actually lands, so
                // "these do not overlap" is a claim that holds: no coverage
                // reaches the content at all.
                AABB overlap;
                const bool overlaps =
                    intersectBoxes(tight, sourceBox, &overlap);
                if (intersectsSource)
                {
                    if (!overlaps)
                    {
                        // Every content texel would be multiplied by zero
                        // coverage. Consume the bracket and draw nothing at
                        // all.
                        return true;
                    }
                    tight = overlap;
                }
                else if (!overlaps)
                {
                    // Inverted, so absent coverage means a factor of one: the
                    // layer is fully visible. Decline the bracket and let it
                    // draw inline, exactly as a source that draws nothing at
                    // all does.
                    return false;
                }
            }
        }

        // Clamped to the artboard only when the artboard actually clips.
        //
        // When it does, content reaching past the edge is cropped at composite
        // time anyway, so rasterizing it would be wasted texels -- and the
        // clamp keeps the raster no larger than the artboard-sized one this
        // replaced, which is what makes the common case monotone: kMaxDim's
        // coarsening can only bite less than before, never more.
        //
        // When it does not clip, that content is genuinely visible, and
        // clamping would crop it inside the mask while it draws fine everywhere
        // else. The artboard-sized rasters had that bug too; this is where it
        // is fixed.
        //
        // The cost is that the monotone guarantee does not hold for an
        // unclipped artboard whose content sprawls far past it: a bigger box
        // can hit kMaxDim and coarsen. Blurrier beats cropped.
        //
        // Skipped entirely for an authored box, where reaching outside the
        // artboard is a decision rather than an accident.
        if (!authoredBox && clip() && !intersectBoxes(tight, bounds(), &tight))
        {
            return false;
        }
    }
    else
    {
        // Nothing changed, so re-measuring would land in the same place. Re-fit
        // the box that was measured, never the box that was fitted -- feeding a
        // fitted box back through the guard band would grow the raster a few
        // texels on every frame.
        tight = mask.m_tightBox;
    }

    // A guard band of transparent texels on every side. Without it the content
    // reaches the raster edge, and the composite's LinearClamp then replicates
    // that edge texel outward under rotation: a one-texel smear along the seam.
    // It is added after the artboard clamp above, so the band can sit a couple
    // of texels outside the artboard -- where the composite is itself clipped
    // when the artboard clips, and where showing those texels is the more
    // correct answer when it does not.
    constexpr uint32_t kGuardTexels = 2;
    if (!offscreen::fitRasterToBox(tight,
                                   kGuardTexels,
                                   offscreen::RasterFit::stableGrid,
                                   &plan))
    {
        return false;
    }

    // Growth is immediate, shrinking waits. Anything rotating or scaling
    // changes the size of its own bounding box every frame, and re-allocating
    // two GPU textures that often costs far more than carrying one bucket of
    // slack.
    const offscreen::RasterResize resize =
        offscreen::decideRasterResize(plan.widthPx,
                                      plan.heightPx,
                                      mask.m_widthPx,
                                      mask.m_heightPx,
                                      &mask.m_shrinkStreak);
    // Hold whatever allocation was settled on, growing the box right and bottom
    // to span it so the composite's inverse still maps exactly onto the pixels.
    offscreen::holdRasterAllocation(resize.widthPx, resize.heightPx, &plan);

    // New textures hold nothing, and the cap inside fitRasterToBox can lower
    // the scale out from under a cached raster, so either forces a rebuild even
    // on a frame that had decided it could reuse what it had.
    const bool mustRaster = contentChanged || resize.reallocate ||
                            plan.rasterScale != mask.m_rasterScale;

    if (mustRaster)
    {
        if (resize.reallocate || mask.m_contentCanvas == nullptr ||
            mask.m_maskCanvas == nullptr)
        {
            mask.m_contentCanvas =
                host->makeContentCanvas(plan.widthPx, plan.heightPx);
            mask.m_maskCanvas =
                host->makeContentCanvas(plan.widthPx, plan.heightPx);
#ifdef TESTING
            mask.testAllocations++;
#endif
        }
        if (mask.m_contentCanvas == nullptr || mask.m_maskCanvas == nullptr)
        {
            mask.releaseCanvases();
            return false;
        }

        // Sequentially, never nested: DeferredSession::endCanvasContent routes
        // back to the screen rather than to an enclosing canvas, so an inner
        // bracket closing inside an outer one would misroute the rest of the
        // outer content. Finishing the coverage canvas before opening the
        // content one keeps the recorded stream two independent segments.
        bool opened = true;
        {
            offscreen::CanvasContentScope scope(host,
                                                mask.m_maskCanvas.get(),
                                                plan,
                                                0);
            if (Renderer* r = scope.renderer())
            {
                // Over the source bracket, not sourceDrawables() directly, so a
                // clipping shape nested inside the source still brackets its
                // own drawables. Starting past the opening marker keeps its
                // own +1 suppression from hiding the very range we want.
                drawDrawableRange(r,
                                  mask.sourceStart()->prevDrawable(),
                                  mask.sourceEnd());
            }
            else
            {
                opened = false;
            }
        }
        if (opened)
        {
            offscreen::CanvasContentScope scope(host,
                                                mask.m_contentCanvas.get(),
                                                plan,
                                                0);
            if (Renderer* r = scope.renderer())
            {
                drawDrawableRange(r, startMarker->prevDrawable(), endMarker);
                // The mask apply has to be the last draw into this canvas: it
                // multiplies everything already there, so anything drawn after
                // it would land unmasked. Issued in the canvas's own pixel
                // space (the scope's transform is still in effect, so undo it)
                // because the coverage texture is the same size as the target
                // and has to line up texel for texel.
                if (rcp<RenderImage> coverage =
                        host->contentCanvasImage(mask.m_maskCanvas.get()))
                {
                    r->save();
                    const float inv = 1.0f / plan.rasterScale;
                    r->translate(plan.box.left(), plan.box.top());
                    r->transform(Mat2D::fromScale(inv, inv));
                    r->applyLayerMask(
                        coverage.get(),
                        ImageSampler::LinearClamp(),
                        static_cast<LayerMaskMode>(mask.maskMode()));
                    r->restore();
                }
            }
            else
            {
                opened = false;
            }
        }
        if (!opened)
        {
            // The host could not open an offscreen frame. Nothing was drawn, so
            // drop the canvases and stay dirty: a later frame retries, and in
            // the meantime the content draws unmasked rather than as a hole.
            mask.releaseCanvases();
            mask.m_dirty = true;
            return false;
        }

        // Commit what the pixels now are. m_tightBox is what a later frame
        // re-fits from; m_box is where these pixels live, which is what the
        // composite has to place them at even after the plan moves on.
        mask.m_tightBox = tight;
        mask.m_box = plan.box;
        mask.m_widthPx = plan.widthPx;
        mask.m_heightPx = plan.heightPx;
        mask.m_rasterScale = plan.rasterScale;
        mask.m_dirty = false;
    }

    rcp<RenderImage> contentImage =
        host->contentCanvasImage(mask.m_contentCanvas.get());
    rcp<RenderImage> coverageImage =
        host->contentCanvasImage(mask.m_maskCanvas.get());
    if (contentImage == nullptr || coverageImage == nullptr)
    {
        return false;
    }

    // mask.m_box, not plan.box: on a frame that reused the cached rasters the
    // plan was re-fitted from scratch, and the pixels being composited belong
    // to whatever box they were rasterized for.
    auto placement = offscreen::beginComposite(renderer,
                                               host,
                                               plan,
                                               mask.m_rasterScale,
                                               mask.m_box);
    placement.renderer->drawImage(contentImage.get(),
                                  ImageSampler::LinearClamp(),
                                  BlendMode::srcOver,
                                  placement.opacity);
    offscreen::endComposite(placement);
    return true;
}
#else
bool Artboard::drawMasked(Renderer*, Drawable*, Drawable*) { return false; }
#endif

#ifdef RIVE_CANVAS
bool Artboard::drawCachedAsBitmap(Renderer* renderer)
{
    // Only reached when m_BitmapCache != nullptr (guarded in drawInternal).
    BitmapCache& cache = *m_BitmapCache;

    if (!cache.cacheEnabled())
    {
        // Authored off (or animated/bound off): behave as though the artboard
        // had no BitmapCache at all. The texture is released by
        // BitmapCache::cacheFlagsChanged when the bit clears.
        return false;
    }

    if (childOpacity() == 0.0f)
    {
        // Fully transparent: nothing to draw, and no need to (re)rasterize.
        // The pending change still has to be consumed the way drawContent()
        // would have on the vector path -- a host that gates frame submission
        // on didChange() (Unity does) otherwise resubmits this invisible
        // artboard every frame, forever. Fold it into the cache rather than
        // dropping it, so content that moved while it was transparent still
        // re-rasterizes when it becomes visible again instead of compositing
        // the raster it had before.
        cache.m_dirty = cache.m_dirty || didChange();
        m_didChange = false;
        return true;
    }

    Factory* f = factory();
    // canvasContentHost, not deferredCanvasHost: this only needs somewhere to
    // rasterize into, and an immediate renderer can provide that without
    // claiming its content is being recorded. The host also allocates the
    // canvas, so nothing here has to go looking for a device.
    auto* deferredHost = f ? f->canvasContentHost() : nullptr;
    // No host means nobody can give us an offscreen frame (a plain non-GPU or
    // test factory), so fall back to a normal vector draw.
    if (deferredHost == nullptr)
    {
        return false;
    }

    // The artboard's own box, which is not [0,0,width,height] in general: with
    // frameOrigin off, bounds() is offset by the origin -- and NestedArtboard
    // turns frameOrigin off on everything it hosts, which is the only way to
    // reach this path. Rasterizing [0,0,w,h] there would capture the wrong
    // region and composite it in the wrong place. bounds() also tracks the
    // resolved layout size rather than the authored width/height.
    const AABB box = bounds();
    const float w = box.width();
    const float h = box.height();
    if (w <= 0.0f || h <= 0.0f)
    {
        return false;
    }

    // drawContent applies the artboard's own rotation/scale, so a self
    // transform ends up baked into the raster -- while the target and the
    // composite below are both sized and placed from the untransformed box.
    // A scaled artboard would be cropped to its original extent, and a rotated
    // one flattened into an axis-aligned texture whose footprint no longer
    // matches what the vector path draws. Caching that correctly needs the
    // target sized from the transformed bounds and the composite placed by the
    // same transform; until then these artboards draw as vectors.
    if (hasSelfTransform())
    {
        return false;
    }

    // Chooses the raster size from the scale the artboard is actually viewed
    // at, and captures the CTM and modulated opacity the composite below needs.
    // See offscreen_raster.hpp for why each of those has to be read here,
    // while recording, rather than at replay.
    offscreen::RasterPlan plan;
    if (!offscreen::planRaster(renderer, box, cache.resolution(), &plan))
    {
        return false;
    }

    // Rebuild when there is no cache, it was explicitly invalidated (resolution
    // changed), the target size or raster scale changed (artboard resized, or
    // the artboard is being viewed at a different zoom), or the content changed
    // this frame.
    bool geomChanged = plan.widthPx != cache.m_widthPx ||
                       plan.heightPx != cache.m_heightPx ||
                       plan.rasterScale != cache.m_rasterScale;
    if (cache.m_canvas == nullptr || cache.m_dirty || geomChanged ||
        didChange())
    {
        renderIntoCanvas(deferredHost, plan);
    }
    if (cache.m_canvas == nullptr)
    {
        return false; // allocation failed; fall back to vector draw.
    }

    // Composite the cached texture in place of the vector content. The
    // renderer's CTM already includes the mount transform; drawImage() spans
    // (widthPx, heightPx), so undoing the raster's fit -- scale back to the
    // box, then move it to the box's corner -- maps a local point (lx,ly) to
    // CTM * (lx,ly), pixel-exact with the vector path.
    // The artboard's own opacity is already baked into the raster (host/render
    // opacity propagate to children and invalidate the cache via didChange()),
    // so the only opacity left to apply is an enclosing modulateOpacity()
    // scope -- and only on the fresh-renderer path, which does not inherit it.
    // Not necessarily the canvas's own image: some backends cannot sample
    // their canvas textures directly and stand in a companion for it.
    rcp<RenderImage> image =
        deferredHost->contentCanvasImage(cache.m_canvas.get());
    if (image == nullptr)
    {
        return false;
    }
    // Picks the renderer to composite through, saves, pixel snaps, and maps the
    // raster's pixel span back onto the box. m_rasterScale, not
    // plan.rasterScale: the raster being composited may have been built on an
    // earlier frame, and the mapping has to invert the scale it was actually
    // drawn at.
    auto placement = offscreen::beginComposite(renderer,
                                               deferredHost,
                                               plan,
                                               cache.m_rasterScale);
    placement.renderer->drawImage(image.get(),
                                  ImageSampler::LinearClamp(),
                                  BlendMode::srcOver,
                                  placement.opacity);
    offscreen::endComposite(placement);
    return true;
}

void Artboard::renderIntoCanvas(cmd::DeferredCanvasHost* deferredHost,
                                const offscreen::RasterPlan& plan)
{
    BitmapCache& cache = *m_BitmapCache;
    // Reuse the texture whenever it is already the right size. This runs on
    // every frame the artboard changes, and on an immediate host each of these
    // is a real GPU allocation -- re-minting one per frame both defeats the
    // point of a cache and thrashes the driver.
    if (cache.m_canvas == nullptr || cache.m_widthPx != plan.widthPx ||
        cache.m_heightPx != plan.heightPx)
    {
        // The host decides whether this needs real pixels now or can defer
        // them to whoever replays.
        cache.m_canvas =
            deferredHost->makeContentCanvas(plan.widthPx, plan.heightPx);
    }
    if (cache.m_canvas == nullptr)
    {
        return;
    }

    // The scope opens the content bracket and applies the raster transform;
    // drawContent (not drawInternal) so we never re-enter the cache hook.
    // clearColor is transparent black.
    bool opened = false;
    {
        offscreen::CanvasContentScope scope(deferredHost,
                                            cache.m_canvas.get(),
                                            plan,
                                            0);
        if (Renderer* r = scope.renderer())
        {
            opened = true;
            drawContent(r);
        }
    }
    if (!opened)
    {
        // The host could not open an offscreen frame (an unbacked canvas, or a
        // host that cannot nest one). Nothing was drawn, so the canvas holds
        // whatever it held before -- uninitialized on the first attempt. Drop
        // it rather than end a bracket that never began: the caller's null
        // check then takes the vector path, and leaving the cache dirty means
        // a later frame retries instead of compositing this hole forever.
        cache.m_canvas.reset();
        cache.m_widthPx = 0;
        cache.m_heightPx = 0;
        cache.m_dirty = true;
        return;
    }

    cache.m_widthPx = plan.widthPx;
    cache.m_heightPx = plan.heightPx;
    cache.m_rasterScale = plan.rasterScale;
    cache.m_dirty = false;
}
#endif

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

// Origin has no dedicated animation/change plumbing, so a live change (e.g. a
// nested artboard origin override, or a keyframed artboard origin) would leave
// the cached render path and layout stale. Invalidate the background/clip path
// (updateRenderPath reads origin) and force the update pass to re-run so the
// content is repositioned. Components dirt is what a hosting NestedArtboard
// keys off (see NestedArtboard::advanceComponent) to re-run our updatePass;
// markHostTransformDirty notifies the host that our transform moved.
void Artboard::originXChanged()
{
    // Base originXChanged is a no-op; go straight to invalidation.
    addDirt(ComponentDirt::Path | ComponentDirt::Components);
    markHostTransformDirty();
}

void Artboard::originYChanged()
{
    addDirt(ComponentDirt::Path | ComponentDirt::Components);
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

// Helper: check if a FocusData has a parent FocusData within the artboard
// by walking up the component hierarchy
static bool hasParentFocusData(const FocusData* focusData)
{
    // FocusData's parent is a ContainerComponent (likely a Node)
    // Walk up to find if any ancestor Node has a FocusData child
    auto* current = focusData->parent();
    while (current != nullptr)
    {
        if (current->is<Node>())
        {
            auto* node = current->as<Node>();
            for (auto child : node->children())
            {
                if (child->is<FocusData>() && child != focusData)
                {
                    return true;
                }
            }
        }
        current = current->parent();
    }
    return false;
}

size_t Artboard::rootFocusDataCount() const
{
    size_t count = 0;
    for (auto* object : m_Objects)
    {
        if (object != nullptr && object->is<FocusData>())
        {
            if (!hasParentFocusData(object->as<FocusData>()))
            {
                count++;
            }
        }
    }
    return count;
}

FocusData* Artboard::rootFocusDataAt(size_t index) const
{
    size_t count = 0;
    for (auto* object : m_Objects)
    {
        if (object != nullptr && object->is<FocusData>())
        {
            if (!hasParentFocusData(object->as<FocusData>()))
            {
                if (count == index)
                {
                    return object->as<FocusData>();
                }
                count++;
            }
        }
    }
    return nullptr;
}

namespace
{
// Depth-first in scene (Container child) order so FocusManager child order
// (tab) matches the hierarchy, not the flat m_Objects / grouped passes.
// At most one FocusData is allowed as a direct child of a container; it is
// registered in a first pass, then child traversal skips that FocusData.
void buildFocusTreeVisit(FocusManager* focusManager,
                         Component* component,
                         rcp<FocusNode> focusNode)
{
    if (component == nullptr)
    {
        return;
    }
    if (component->is<NestedArtboard>())
    {
        auto* nestedHost = component->as<NestedArtboard>();
        // Wire the nested state machines to this manager before placing the
        // scope: setExternalFocusManager rebuilds the nested focus tree at the
        // manager root, so it must run first. Track whether any was actually
        // re-wired — a re-wire leaves the tree at the root and requires
        // re-homing under the scope (forceRebuild); an already-wired tree is
        // left in place.
        bool rewired = false;
        for (auto* animation : nestedHost->nestedAnimations())
        {
            if (animation->is<NestedStateMachine>())
            {
                auto* nsm = animation->as<NestedStateMachine>();
                auto* smi = nsm->stateMachineInstance();
                if (smi != nullptr && smi->focusManager() != focusManager)
                {
                    smi->setExternalFocusManager(focusManager);
                    rewired = true;
                }
            }
        }
        // Scope placement must be the final write so it overrides the root
        // rebuild above. For data-bound hosts this parents the nested focus
        // tree under the host's persistent structural scope; static hosts
        // build directly under focusNode. placeScope=true: the build pass is
        // the ordering authority — the scope is re-appended at the walk's
        // position.
        nestedHost->syncNestedFocusTree(focusNode,
                                        /*placeScope=*/true,
                                        /*forceRebuild=*/rewired);
    }
    else if (component->is<ArtboardComponentList>())
    {
        auto* list = component->as<ArtboardComponentList>();
        list->ensureListScopeFocusNode(focusManager, focusNode);
    }
    if (component->is<ContainerComponent>())
    {
        auto* cc = component->as<ContainerComponent>();
        FocusData* directFd = nullptr;
        for (Component* ch : cc->children())
        {
            if (ch != nullptr && ch->is<FocusData>())
            {
                directFd = ch->as<FocusData>();
                break;
            }
        }
        rcp<FocusNode> recurseWith;
        if (directFd != nullptr)
        {
            focusManager->addChild(focusNode, directFd->focusNode());
            recurseWith = directFd->focusNode();
        }
        else
        {
            recurseWith = focusNode;
        }
        for (Component* ch : cc->children())
        {
            if (ch == nullptr || ch->is<FocusData>())
            {
                continue;
            }
            buildFocusTreeVisit(focusManager, ch, recurseWith);
        }
    }
}
} // namespace

FocusManager* Artboard::ensureFocusManager()
{
    if (m_activeFocusManager != nullptr)
    {
        return m_activeFocusManager;
    }
    if (m_ownedFocusManager == nullptr)
    {
        m_ownedFocusManager = std::make_unique<FocusManager>();
    }
    m_activeFocusManager = m_ownedFocusManager.get();
    return m_activeFocusManager;
}

void Artboard::adoptFocusManager(FocusManager* manager)
{
    // A null manager means "stop borrowing", which falls back to our own if we
    // have one — the same shape as clearing the old external-manager override.
    if (manager == nullptr)
    {
        manager = m_ownedFocusManager.get();
    }
    if (m_activeFocusManager == manager)
    {
        return;
    }

    if (m_activeFocusManager != nullptr)
    {
        cleanupFocusTree();
    }

    m_activeFocusManager = manager;

    if (manager != nullptr)
    {
        buildFocusTree(manager, nullptr);
    }
}

void Artboard::buildFocusTree(FocusManager* focusManager,
                              rcp<FocusNode> parentFocusNode)
{
    if (focusManager == nullptr)
    {
        return;
    }

    // Store reference to the active focus manager
    setActiveFocusManager(focusManager);

#ifdef WITH_RIVE_TOOLS
    // Store the parent focus node if provided (for later retrieval by tools)
    if (parentFocusNode != nullptr)
    {
        m_externalParentFocusNode = parentFocusNode;
    }
    // Use explicit parent if provided, otherwise fall back to external parent
    rcp<FocusNode> effectiveParent = parentFocusNode != nullptr
                                         ? parentFocusNode
                                         : m_externalParentFocusNode;
#else
    rcp<FocusNode> effectiveParent = parentFocusNode;
#endif

    if (as<ContainerComponent>() != nullptr)
    {
        buildFocusTreeVisit(focusManager, this, std::move(effectiveParent));
    }
}

void Artboard::buildFocusTree(rcp<FocusNode> parentFocusNode)
{
    if (parentFocusNode == nullptr)
    {
        return;
    }
    auto* manager = parentFocusNode->manager();
    if (manager == nullptr)
    {
        return;
    }
    buildFocusTree(manager, parentFocusNode);
}

void Artboard::cleanupFocusTree()
{
    if (m_activeFocusManager == nullptr)
    {
        return;
    }

    // Remove all FocusData's FocusNodes from the FocusManager
    for (auto* obj : m_Objects)
    {
        if (obj != nullptr && obj->is<FocusData>())
        {
            auto* fd = obj->as<FocusData>();
            auto node = fd->focusNode();
            if (node == nullptr)
            {
                // FocusNode never lazily created — nothing to remove.
                continue;
            }
            // Remove the node when it belongs to this manager. Nodes owned by
            // a DIFFERENT live manager are left untouched.
            if (node->manager() == m_activeFocusManager)
            {
                m_activeFocusManager->removeChild(node);
            }
            else if (node->manager() == nullptr && node->parent() != nullptr)
            {
                // The node's manager was nulled while it is still attached —
                // either by an ancestor removal (removeChild clears m_manager
                // across a removed subtree) or by ~FocusManager, which nulls
                // m_manager tree-wide as it dies. Detach node-side only: in the
                // second case m_activeFocusManager is already dangling (it is
                // an unowned back-pointer, see setActiveFocusManager) and
                // touching it is a use-after-free. Nothing is lost by skipping
                // markFocusableContentDirty here — if the manager is alive, the
                // ancestor removal that nulled m_manager already marked it.
                node->removeFromParent();
            }
        }
    }

    // Propagate cleanup to nested artboards that share our FocusManager
    for (auto* nestedArtboardHost : m_NestedArtboards)
    {
        auto* nestedArtboard = nestedArtboardHost->artboardInstance(0);
        if (nestedArtboard != nullptr &&
            nestedArtboard->focusManager() == m_activeFocusManager)
        {
            nestedArtboard->cleanupFocusTree();
        }
    }

    // Propagate cleanup to ArtboardComponentList items
    for (auto* componentList : m_ComponentLists)
    {
        for (size_t i = 0; i < componentList->artboardCount(); i++)
        {
            auto* nestedArtboard =
                componentList->artboardInstance(static_cast<int>(i));
            if (nestedArtboard != nullptr &&
                nestedArtboard->focusManager() == m_activeFocusManager)
            {
                nestedArtboard->cleanupFocusTree();
            }
        }
    }

    for (auto* componentList : m_ComponentLists)
    {
        componentList->removeListScopeFocusNode();
    }

    // Fall back to our own manager rather than to "no manager"
    m_activeFocusManager = m_ownedFocusManager.get();
}

#ifdef WITH_RIVE_TOOLS
void Artboard::setExternalParentFocusNode(rcp<FocusNode> node)
{
    m_externalParentFocusNode = std::move(node);
}

rcp<FocusNode> Artboard::externalParentFocusNode() const
{
    return m_externalParentFocusNode;
}

void Artboard::collapseSingle(bool value) { Component::collapse(value); }
#endif

// Builds the semantic tree for this artboard. Iterates all objects,
// registers each SemanticData's SemanticNode with the manager, then
// propagates to nested artboards using findClosestSemanticNode() for
// parent resolution across artboard boundaries.
//
// For nested artboards (those with a host), a boundary SemanticNode is
// created and inserted as the parent of all semantic nodes within this
// artboard. Boundary nodes are structural-only — skipped during
// flattening — but enable better subtree collapse/uncollapse and
// targeted bounds dirtying when the host transform changes.
void Artboard::buildSemanticTree(SemanticManager* semanticManager,
                                 rcp<SemanticNode> parentSemanticNode)
{
    if (semanticManager == nullptr)
    {
        return;
    }

    // Store reference to the active semantic manager
    setActiveSemanticManager(semanticManager);

    // For nested artboards, create a boundary node that acts as the
    // structural root of this artboard's semantic subtree.
    rcp<SemanticNode> effectiveParent = parentSemanticNode;
    if (host() != nullptr)
    {
        if (m_semanticBoundaryNode == nullptr)
        {
            // Id is assigned by the SemanticManager on addChild() below.
            m_semanticBoundaryNode = rcp<SemanticNode>(new SemanticNode());
            m_semanticBoundaryNode->isBoundaryNode(true);
            m_semanticBoundaryNode->boundaryArtboard(this);
        }
        semanticManager->addChild(parentSemanticNode, m_semanticBoundaryNode);
        // Seed the boundary as dirty so the first drainDiff() resolves its
        // bounds from the artboard rect even when semantics are enabled on
        // an already-settled scene (no subsequent WorldTransform dirt to
        // trigger this via the normal update cycle).
        markSemanticBoundaryTransformDirty();
        effectiveParent = m_semanticBoundaryNode;
    }

    // Register all SemanticData in this artboard
    for (auto* obj : m_Objects)
    {
        if (obj != nullptr && obj->is<SemanticData>())
        {
            auto* sd = obj->as<SemanticData>();
            auto* localParent = sd->findParentSemanticData();

            rcp<SemanticNode> parentNode = localParent != nullptr
                                               ? localParent->semanticNode()
                                               : effectiveParent;

            semanticManager->addChild(parentNode, sd->semanticNode());
            sd->syncSemanticTreeVisibility();
        }
    }

    // Propagate semantic registration to nested artboards that might have been
    // created before this artboard's semanticManager was available.
    //
    // We check if the nested artboard's semanticManager is DIFFERENT from ours.
    // If it is, that means it created its own internal semanticManager when it
    // should be sharing the parent's. We rebuild its semantic tree with the
    // correct shared semanticManager.

    // Handle NestedArtboard instances
    for (auto* nestedArtboardHost : m_NestedArtboards)
    {
        // Find closest semantic node (handles artboard boundaries)
        auto hostParentNode =
            SemanticData::findClosestSemanticNode(nestedArtboardHost);
        if (hostParentNode == nullptr)
        {
            hostParentNode = effectiveParent;
        }

        auto* nestedArtboard = nestedArtboardHost->artboardInstance(0);
        if (nestedArtboard != nullptr &&
            nestedArtboard->semanticManager() != semanticManager)
        {
            // Clean up old semantic tree if it exists (with wrong
            // semanticManager)
            nestedArtboard->cleanupSemanticTree();
            nestedArtboard->buildSemanticTree(semanticManager, hostParentNode);
        }
    }

    // Handle ArtboardComponentList instances
    for (auto* componentList : m_ComponentLists)
    {
        // Find closest semantic node (handles artboard boundaries)
        auto hostParentNode =
            SemanticData::findClosestSemanticNode(componentList);
        if (hostParentNode == nullptr)
        {
            hostParentNode = effectiveParent;
        }

        for (size_t i = 0; i < componentList->artboardCount(); i++)
        {
            auto* nestedArtboard =
                componentList->artboardInstance(static_cast<int>(i));
            if (nestedArtboard != nullptr &&
                nestedArtboard->semanticManager() != semanticManager)
            {
                // Clean up old semantic tree if it exists (with wrong
                // semanticManager)
                nestedArtboard->cleanupSemanticTree();
                nestedArtboard->buildSemanticTree(semanticManager,
                                                  hostParentNode);
            }
        }
    }
}

void Artboard::cleanupSemanticTree()
{
    if (m_activeSemanticManager == nullptr)
    {
        return;
    }

    // Propagate cleanup to nested artboards that share our SemanticManager.
    // Done FIRST so their boundary nodes are removed before we remove ours.
    for (auto* nestedArtboardHost : m_NestedArtboards)
    {
        auto* nestedArtboard = nestedArtboardHost->artboardInstance(0);
        if (nestedArtboard != nullptr &&
            nestedArtboard->semanticManager() == m_activeSemanticManager)
        {
            nestedArtboard->cleanupSemanticTree();
        }
    }

    // Propagate cleanup to ArtboardComponentList items
    for (auto* componentList : m_ComponentLists)
    {
        for (size_t i = 0; i < componentList->artboardCount(); i++)
        {
            auto* nestedArtboard =
                componentList->artboardInstance(static_cast<int>(i));
            if (nestedArtboard != nullptr &&
                nestedArtboard->semanticManager() == m_activeSemanticManager)
            {
                nestedArtboard->cleanupSemanticTree();
            }
        }
    }

    // Remove all SemanticData's SemanticNodes from the SemanticManager.
    // Use hasSemanticNode() to avoid lazy-creating a node during cleanup.
    for (auto* obj : m_Objects)
    {
        if (obj != nullptr && obj->is<SemanticData>())
        {
            auto* sd = obj->as<SemanticData>();
            if (!sd->hasSemanticNode())
            {
                continue;
            }
            auto node = sd->semanticNode();
            if (node != nullptr && node->manager() == m_activeSemanticManager)
            {
                m_activeSemanticManager->removeChild(node);
            }
        }
    }

    // Remove the boundary node for this artboard (if any).
    if (m_semanticBoundaryNode != nullptr &&
        m_semanticBoundaryNode->manager() == m_activeSemanticManager)
    {
        m_activeSemanticManager->removeChild(m_semanticBoundaryNode);
    }
    m_semanticBoundaryNode = nullptr;

    // Clear the active semantic manager reference
    m_activeSemanticManager = nullptr;
}

// Walk the semantic subtree under a boundary node and collapse each
// SemanticData directly via the back-pointer. O(K) where K = semantic
// nodes under this boundary.
static void collapseBoundarySubtree(SemanticNode* node, bool value)
{
    // Copy children before iterating — collapse(true) calls
    // removeChild which mutates the original vector.
    std::vector<rcp<SemanticNode>> children(node->children());
    for (const auto& child : children)
    {
        auto* sd = child->semanticData();
        if (sd != nullptr && sd->isCollapsed() != value)
        {
            sd->collapse(value);
        }
        collapseBoundarySubtree(child.get(), value);
    }
}

// Semantic-only collapse for this artboard's semantic nodes.
// When collapsing, walks the boundary's semantic subtree (O(K) semantic
// nodes). When uncollapsing, falls back to m_Objects because
// SemanticData::collapse(false) re-parents nodes outside the boundary.
void Artboard::collapseSemanticBoundary(bool value)
{
    if (m_activeSemanticManager == nullptr)
    {
        return;
    }
    if (value && m_semanticBoundaryNode != nullptr)
    {
        // Fast path: walk the boundary tree.
        collapseBoundarySubtree(m_semanticBoundaryNode.get(), true);
    }
    else
    {
        // Uncollapse or no boundary: scan artboard objects.
        for (auto* obj : m_Objects)
        {
            if (obj != nullptr && obj->is<SemanticData>())
            {
                auto* sd = obj->as<SemanticData>();
                if (sd->isCollapsed() != value)
                {
                    sd->collapse(value);
                }
            }
        }
    }

    if (!value)
    {
        markSemanticBoundaryTransformDirty();
    }
}

// Recursively mark all semantic nodes under the boundary as bounds-dirty.
// Called when the host transform changes so bounds are recalculated in
// root artboard space.
void Artboard::markSemanticBoundaryTransformDirty()
{
    if (m_semanticBoundaryNode == nullptr || m_activeSemanticManager == nullptr)
    {
        return;
    }
    m_activeSemanticManager->markBoundaryDirty(m_semanticBoundaryNode->id());
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

void Artboard::buildDataContext(rcp<DataContext> value) {}

void Artboard::internalDataContext(rcp<DataContext> value)
{
    // Set the context before recursing into the artboard hosts; they read it
    // back off this artboard while they bind. The binds are walked after.
    dataBindContext(value);
    syncInstanceValueBinds();
    for (auto artboardHost : m_ArtboardHosts)
    {
        auto hostValue =
            value->getViewModelInstance(artboardHost->dataBindPath());
        if (hostValue != nullptr && hostValue->is<ViewModelInstance>())
        {
            artboardHost->bindViewModelInstance(hostValue, value);
        }
        else
        {
            artboardHost->internalDataContext(value);
        }
    }
    bindDataBindsFromContext();
    sortDataBinds();
    for (auto* scriptedObject : m_ScriptedObjects)
    {
        scriptedObject->dataContext(value);
    }
    initScriptedObjects();
}

void Artboard::rebind() { internalDataContext(dataBindContext()); }

void Artboard::relinkDataContext()
{
    wakeIfQuietRow();
    if (dataBindContext() == nullptr)
    {
        return;
    }
    for (auto artboardHost : m_ArtboardHosts)
    {
        rcp<ViewModelInstance> value = dataBindContext()->getViewModelInstance(
            artboardHost->dataBindPath());
        if (value == nullptr)
        {
            value = dataBindContext()->mainViewModelInstance();
        }
        artboardHost->relinkDataContext(value);
    }
}

void Artboard::rebuildDataBind(DataBind* dataBind)
{
    if (dataBind->is<DataBindContext>())
    {
        dataBind->as<DataBindContext>()->bindFromContext(
            dataBindContext().get());
    }
};

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
    if (dataBindContext() != nullptr)
    {
        dataBindContext()->removeDependentContainer(this);
        dataBindContext(nullptr);
        syncInstanceValueBinds();
    }
    for (auto artboardHost : m_ArtboardHosts)
    {
        artboardHost->clearDataContext();
    }
    for (auto* scriptedObject : m_ScriptedObjects)
    {
        scriptedObject->resetLuaInit();
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

void Artboard::hostOpacity(float value)
{
    if (m_hostOpacity == value)
    {
        return;
    }
    m_hostOpacity = value;
    // Re-propagate opacity to our contents. The property itself is untouched,
    // so we dirty render opacity directly rather than through opacity(...).
    addDirt(ComponentDirt::RenderOpacity, true);
}

void Artboard::dataContext(rcp<DataContext> value)
{
    internalDataContext(value);
}

rcp<const File> Artboard::artboardFile() const { return nullptr; }

void Artboard::bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance)
{
    bindViewModelInstance(viewModelInstance, nullptr);
}

void Artboard::bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                                     rcp<DataContext> parent)
{
    if (viewModelInstance == nullptr)
    {
        unbind();
        return;
    }
    setViewModelInstance(std::move(viewModelInstance));
    if (parent != nullptr && dataBindContext() != nullptr)
    {
        dataBindContext()->parent(parent);
    }
    bind();
}

void Artboard::setViewModelInstance(rcp<ViewModelInstance> viewModelInstance)
{
    if (viewModelInstance == nullptr)
    {
        return;
    }
    if (dataBindContext() == nullptr)
    {
        dataBindContext(make_rcp<DataContext>(viewModelInstance));
        dataBindContext()->addDependentContainer(this);
        return;
    }
    // The data context re-points every attached container (this artboard and
    // any state machines sharing the context) off the old main and onto the new
    // one.
    dataBindContext()->setMainViewModelInstance(viewModelInstance);
}

void Artboard::bindViewModelInstances(
    std::vector<rcp<ViewModelInstance>> viewModelInstances,
    rcp<DataContext> parent)
{
    if (viewModelInstances.empty())
    {
        unbind();
        return;
    }
    clearDataContext();
    auto dataContext = make_rcp<DataContext>(std::move(viewModelInstances));
    dataContext->addDependentContainer(this);
    dataContext->parent(parent);
    internalDataContext(dataContext);
}

void Artboard::bind()
{
    if (dataBindContext() != nullptr)
    {
        internalDataContext(dataBindContext());
    }
}

rcp<ViewModelInstance> Artboard::globalViewModelInstance(
    const std::string& name)
{
    // Pure read: returns the instance in the named slot only if one has been
    // set/bound; never creates.
    if (dataBindContext() == nullptr)
    {
        return nullptr;
    }
    auto f = artboardFile();
    if (f == nullptr)
    {
        return nullptr;
    }
    return dataBindContext()->instanceForSlot(f->viewModelId(name));
}

bool Artboard::setGlobalViewModelInstance(
    const std::string& name,
    rcp<ViewModelInstance> viewModelInstance)
{
    // A null instance is allowed: it empties the named slot below.
    auto f = artboardFile();
    if (f == nullptr)
    {
        return false;
    }
    // The slot is addressed by the named view model (its file index), not by
    // the instance's own view model — so an override instance of a different
    // view model can be placed on the slot.
    uint32_t slotKey = f->viewModelId(name);
    if (slotKey >= f->viewModelCount())
    {
        return false;
    }
    // Only global view models get a slot; a non-global name is not a valid
    // global slot and must not be slotted.
    auto slotViewModel = f->viewModel(slotKey);
    if (slotViewModel == nullptr ||
        static_cast<ViewModelType>(slotViewModel->viewModelType()) !=
            ViewModelType::global)
    {
        return false;
    }
    if (dataBindContext() == nullptr)
    {
        // Nothing to clear when there is no context yet; only create one when
        // actually placing an instance.
        if (viewModelInstance == nullptr)
        {
            return true;
        }
        dataBindContext(make_rcp<DataContext>(rcp<ViewModelInstance>(nullptr)));
        dataBindContext()->addDependentContainer(this);
    }
    // The data context re-points every attached container off any previous
    // instance occupying this slot and onto the new one (or empties the slot
    // when the instance is null).
    dataBindContext()->setViewModelInstanceForSlot(slotKey, viewModelInstance);
    return true;
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

void Artboard::changed()
{
    if (!m_didChange)
    {
        m_didChange = true;
        if (parentArtboard())
        {
            parentArtboard()->changed();
        }
    }
}

////////// ArtboardInstance

#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"

ArtboardInstance::ArtboardInstance() {}

ArtboardInstance::~ArtboardInstance() {}

void ArtboardInstance::file(rcp<const File> file) { m_file = std::move(file); }

rcp<const File> ArtboardInstance::file() const { return m_file; }

rcp<const File> ArtboardInstance::artboardFile() const { return m_file; }

std::unique_ptr<LinearAnimationInstance> ArtboardInstance::animationAt(
    size_t index)
{
    auto la = this->animation(index);
    return la ? std::make_unique<LinearAnimationInstance>(la, this) : nullptr;
}

std::unique_ptr<LinearAnimationInstance> ArtboardInstance::animationNamed(
    const std::string& name)
{
    auto la = this->animation(name);
    return la ? std::make_unique<LinearAnimationInstance>(la, this) : nullptr;
}

std::unique_ptr<StateMachineInstance> ArtboardInstance::stateMachineAt(
    size_t index)
{
    auto sm = this->stateMachine(index);
    if (sm == nullptr)
    {
        return nullptr;
    }
    auto smInstance = std::make_unique<StateMachineInstance>(sm, this);
    if (auto dc = dataContext())
    {
        smInstance->inheritDataContext(dc);
    }
    return smInstance;
}

std::unique_ptr<StateMachineInstance> ArtboardInstance::stateMachineNamed(
    const std::string& name)
{
    auto sm = this->stateMachine(name);
    if (sm == nullptr)
    {
        return nullptr;
    }
    auto smInstance = std::make_unique<StateMachineInstance>(sm, this);
    if (auto dc = dataContext())
    {
        smInstance->inheritDataContext(dc);
    }
    return smInstance;
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
