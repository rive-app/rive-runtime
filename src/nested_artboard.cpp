#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_origin.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/file.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/focusable.hpp"
#include "rive/nested_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/data_bind/data_bind_path.hpp"
#include "rive/clip_result.hpp"
#include "rive/text/text_input.hpp"
#include "rive/view_model_type.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include <limits>
#include <cassert>

using namespace rive;

namespace
{
std::vector<rcp<ViewModelInstance>> buildVMIList(
    const rcp<ViewModelInstance>& primary,
    const std::vector<rcp<ViewModelInstance>>& globals)
{
    std::vector<rcp<ViewModelInstance>> list;
    list.reserve((primary != nullptr ? 1 : 0) + globals.size());
    if (primary != nullptr)
    {
        list.push_back(primary);
    }
    for (auto& g : globals)
    {
        if (g != nullptr)
        {
            list.push_back(g);
        }
    }
    return list;
}
} // namespace

NestedArtboard::NestedArtboard() {}
NestedArtboard::~NestedArtboard()
{
    // Release dependencies of nested animations BEFORE m_Instance is destroyed.
    // The nested animations (like NestedStateMachine) hold
    // StateMachineInstances that reference m_Instance. If we don't release them
    // here, their destructors (called later when the parent artboard destroys
    // them from m_Objects) will try to access the already-freed m_Instance.
    for (auto& animation : m_NestedAnimations)
    {
        animation->releaseDependencies();
    }
    // Also release the bound state machine's dependencies if it exists
    if (m_boundNestedStateMachine)
    {
        m_boundNestedStateMachine->releaseDependencies();
    }

    m_viewModelInstance = nullptr;
    // Release the owned bound VMI. The borrowed stateful child VMI is cleaned
    // up by the parent Artboard's m_Objects teardown.
    if (m_ownsActiveVmi && m_activeViewModelInstance != nullptr)
    {
        m_activeViewModelInstance->unref();
    }
    m_activeViewModelInstance = nullptr;
    m_ownsActiveVmi = false;
    // Release our extra refs on the global VMI children (the artboard's
    // m_Objects still holds its own ref for each).
    m_globalViewModelInstances.clear();

    // Scope outlives nested instance swaps; remove from parent FocusManager
    // when the host component is destroyed.
    if (m_focusScope != nullptr && m_focusScope->manager() != nullptr)
    {
        m_focusScope->manager()->removeChild(m_focusScope);
    }
    m_focusScope = nullptr;
}

Core* NestedArtboard::clone() const
{
    NestedArtboard* nestedArtboard =
        static_cast<NestedArtboard*>(NestedArtboardBase::clone());
    nestedArtboard->file(file());
    // Carry the swap-slot flag to clones. It is detected on the SOURCE
    // artboard's host (import populates the source's data binds before
    // onAddedClean)
    if (isArtboardDataBound())
    {
        nestedArtboard->m_hostFlags |=
            NestedArtboardHostFlags::artboardDataBound;
    }
    if (m_referencedArtboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_referencedArtboard->instance();
    nestedArtboard->referencedArtboard(ni.release());
    return nestedArtboard;
}

void NestedArtboard::nest(Artboard* artboard)
{
    m_referencedArtboard = artboard;
    if (!m_referencedArtboard->isInstance())
    {
        // We're just marking the source artboard so we can later instance from
        // it. No need to advance it or change any of its properties.
        // E.g. at import time, we return here.
        return;
    }
    m_referencedArtboard->frameOrigin(false);
    m_referencedArtboard->opacity(renderOpacity());
    m_referencedArtboard->volume(artboard->volume());
    m_Instance = nullptr;
    if (artboard->isInstance())
    {
        m_Instance.reset(
            static_cast<ArtboardInstance*>(artboard)); // take ownership
    }
    // This allows for swapping after initial load (after onAddedClean has
    // already been called).
    m_referencedArtboard->host(this);
    // Re-push any authored origin override onto the freshly mounted instance.
    // On initial import the override child may not be linked yet; onAddedClean
    // covers that case.
    applyOriginOverride();
}

void NestedArtboard::applyOriginOverride()
{
    if (m_referencedArtboard == nullptr || !m_referencedArtboard->isInstance())
    {
        return;
    }
    for (auto child : children())
    {
        if (child->is<NestedArtboardOrigin>())
        {
            auto origin = child->as<NestedArtboardOrigin>();
            m_referencedArtboard->originX(origin->originX());
            m_referencedArtboard->originY(origin->originY());
            return;
        }
    }
}

bool NestedArtboard::tryScheduleBindStateful()
{

    if (m_activeViewModelInstance != nullptr && artboardInstance())
    {
        m_hostFlags |= NestedArtboardHostFlags::pendingStatefulBinding;
        return true;
    }
    return false;
}

void NestedArtboard::bindStateful()
{
    m_hostFlags &= ~NestedArtboardHostFlags::pendingStatefulBinding;
    if (artboardInstance() == nullptr)
    {
        return;
    }
    // Active VMI is the local-root binding; append the global VMI children.
    auto list = buildVMIList(ref_rcp(m_activeViewModelInstance),
                             m_globalViewModelInstances);
    artboardInstance()->bindViewModelInstances(std::move(list), m_dataContext);
    for (auto& animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>())
        {
            animation->as<NestedStateMachine>()->dataContext(
                artboardInstance()->dataContext());
        }
    }
}

ViewModelInstance* NestedArtboard::findStatefulChildVmi() const
{
    for (auto child : children())
    {
        if (child->is<ViewModelInstance>())
        {
            return child->as<ViewModelInstance>();
        }
    }
    return nullptr;
}

void NestedArtboard::setActiveViewModelInstance(ViewModelInstance* vmi,
                                                bool owns)
{
    if (m_activeViewModelInstance == vmi)
    {
        // Same pointer; tolerate an ownership refresh only when both states
        // agree it's still owned (idempotent reuse of the bound instance).
        return;
    }
    if (m_ownsActiveVmi && m_activeViewModelInstance != nullptr)
    {
        m_activeViewModelInstance->unref();
    }
    m_activeViewModelInstance = vmi;
    m_ownsActiveVmi = owns;
}

void NestedArtboard::clearNestedAnimations()
{
    for (auto& animation : m_NestedAnimations)
    {
        // Release the nested animation dependencies. The file will take care of
        // destroying the nested animation itself.
        animation->releaseDependencies();
    }
    m_NestedAnimations.clear();
}

void NestedArtboard::updateArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
    // A VM artboard swap means this host is data-bound.
    m_hostFlags |= NestedArtboardHostFlags::artboardDataBound;

    // Resolve the swap target BEFORE tearing anything down. asset == nullptr
    // and propertyValue == -1 is the user explicitly clearing the artboard (a
    // real "set to null"); any OTHER unresolvable id must leave the currently
    // displayed artboard — and its focus tree — untouched, rather than blanking
    // focus for content that stays on screen. findArtboard has no side effects,
    // so resolving it early is safe.
    bool explicitNull = viewModelInstanceArtboard != nullptr &&
                        viewModelInstanceArtboard->asset() == nullptr &&
                        viewModelInstanceArtboard->propertyValue() == -1;
    Artboard* artboard =
        explicitNull
            ? nullptr
            : findArtboard(viewModelInstanceArtboard, parentArtboard(), m_file);
    if (!explicitNull && artboard == nullptr)
    {
        // Unresolved target — keep the outgoing instance and its focus intact.
        return;
    }

    // Detach the outgoing instance's focus tree BEFORE teardown, while every
    // component is still alive: clearing focus runs blur callbacks
    // (FocusData::blurred walks parents/siblings), which must act on live
    // components rather than partially-destroyed ones.
    auto* outgoing = artboardInstance(0);
    if (outgoing != nullptr)
    {
        outgoing->cleanupFocusTree();
    }
    clearDataContext();
    clearNestedAnimations();
    m_boundNestedStateMachine = nullptr;

    if (explicitNull)
    {
        if (m_referencedArtboard)
        {
            m_referencedArtboard->host(nullptr);
            m_referencedArtboard = nullptr;
        }
        m_Instance = nullptr;
        setActiveViewModelInstance(nullptr, false);
        return;
    }

    if (artboard != nullptr)
    {
        auto artboardInstance = artboard->instance();
        if (artboard->stateMachineCount() > 0)
        {

            auto nestedStateMachine = new NestedStateMachine();
            nestedStateMachine->animationId(0);
            nestedStateMachine->initializeAnimation(artboardInstance.get());
            addNestedAnimation(nestedStateMachine);

            m_boundNestedStateMachine.reset(static_cast<NestedStateMachine*>(
                nestedStateMachine)); // take ownership
        }
        referencedArtboard(artboardInstance.release());

        if (isStateful())
        {
            auto statefulChild = findStatefulChildVmi();
            if (statefulChild != nullptr &&
                statefulChild->viewModelId() == artboard->viewModelId())
            {
                // Source artboard uses the same VM as the stateful child:
                // borrow the child instance.
                setActiveViewModelInstance(statefulChild, false);
            }
            else if (m_ownsActiveVmi && m_activeViewModelInstance != nullptr &&
                     m_activeViewModelInstance->viewModelId() ==
                         artboard->viewModelId())
            {
                // Already holding a bound instance for this VM; reuse.
            }
            else
            {
                auto vm = m_file != nullptr
                              ? m_file->viewModel(artboard->viewModelId())
                              : nullptr;
                rcp<ViewModelInstance> newBound =
                    vm != nullptr ? m_file->createDefaultViewModelInstance(vm)
                                  : nullptr;
                ViewModelInstance* raw = newBound.release();
                setActiveViewModelInstance(raw, /*owns=*/raw != nullptr);
                if (raw != nullptr)
                {
                    m_file->completeViewModelProperties(raw);
                }
            }
        }
        else
        {
            setActiveViewModelInstance(nullptr, false);
        }

        if (viewModelInstanceArtboard->boundViewModelInstance())
        {
            bindViewModelInstance(
                viewModelInstanceArtboard->boundViewModelInstance(),
                m_dataContext);
        }
        else if (tryScheduleBindStateful())
        {
            bindStateful();
        }
        else if (m_dataContext != nullptr && m_viewModelInstance == nullptr)
        {
            internalDataContext(m_dataContext);
        }
        else if (m_viewModelInstance != nullptr)
        {
            bindViewModelInstance(m_viewModelInstance, m_dataContext);
        }
        // TODO: @hernan review what dirt to add
        addDirt(ComponentDirt::Filthy);

        // Share the parent's focus manager with the swapped-in artboard's
        // state machine BEFORE the focus sync: its listener groups (pointer
        // focus, key/text input) must act on the same focus state Tab
        // traversal uses. The wiring in
        // NestedStateMachine::initializeAnimation is skipped here because the
        // runtime-created bound state machine is unparented.
        auto* parentAb = parentArtboard();
        auto* parentFM =
            parentAb != nullptr ? parentAb->focusManager() : nullptr;
        if (parentFM != nullptr && m_boundNestedStateMachine != nullptr)
        {
            auto* smi = m_boundNestedStateMachine->stateMachineInstance();
            if (smi != nullptr && smi->focusManager() != parentFM)
            {
                smi->setExternalFocusManager(parentFM);
            }
        }
        // Re-home the nested instance's focus tree under the host's scope.
        // setExternalFocusManager above rebuilds the nested tree at the
        // manager root, so scope placement must be the final write.
        syncNestedFocusTree(FocusData::findClosestFocusNode(this));
    }
}

// Focus tree integration for data-bound nested artboards.
//
// When artboardId is bound to a VM artboard property the nested instance can be
// swapped at runtime (e.g. Plain -> Focusable), so its FocusData leaves must
// live in the *parent* StateMachine's FocusManager for Tab order to work across
// the main artboard and nested content. Each data-bound host owns one
// structural m_focusScope (see registerFocusScope) that persists across swaps;
// static nested artboards get no scope and zero extra FocusNode cost.

void NestedArtboard::detectArtboardDataBinding()
{
    // Marks hosts that can receive runtime
    // artboard swaps so a focus scope is allocated later at tree-build time.
    // Only a direct bind of this host's artboardId counts. Match on the
    // bind's target property key: it is static file data, available at
    // import time on the source artboard
    if (isArtboardDataBound())
    {
        return;
    }
    auto* parent = artboard();
    if (parent != nullptr)
    {
        for (auto* dataBind : parent->dataBinds())
        {
            if (dataBind->target() == this &&
                dataBind->propertyKey() ==
                    NestedArtboardBase::artboardIdPropertyKey)
            {
                m_hostFlags |= NestedArtboardHostFlags::artboardDataBound;
                return;
            }
        }
    }
}

void NestedArtboard::registerFocusScope(FocusManager* focusManager,
                                        rcp<FocusNode> parentNode,
                                        bool place)
{
    // No-op for static hosts; data-bound hosts lazily get one persistent scope.
    if (focusManager == nullptr || !isArtboardDataBound())
    {
        return;
    }

    if (m_focusScope == nullptr)
    {
        m_focusScope = FocusNode::makeStructuralScope();
    }

    if (!place && m_focusScope->manager() == focusManager)
    {
        // Ensure-only callers (state machine init, artboard swap) must never
        // move an already-registered scope: the full build pass is the only
        // ordering authority.
        return;
    }

    // Build pass placement, or first registration. During the depth-first
    // build pass every sibling that should precede this scope has already been
    // re-appended by the same walk, so appending yields hierarchy order. A
    // first registration outside a pass is best-effort (appended under the
    // caller's fallback parent) and is normalized by the next build pass.
    focusManager->addChild(std::move(parentNode), m_focusScope);
}

void NestedArtboard::syncNestedFocusTree(rcp<FocusNode> fallbackParent,
                                         bool placeScope,
                                         bool forceRebuild)
{
    auto* parentAb = artboard();
    auto* parentFM = parentAb != nullptr ? parentAb->focusManager() : nullptr;
    if (parentFM == nullptr)
    {
        return;
    }

    // Place the structural scope first — even with no nested instance yet — so
    // a data-bound host whose artboard starts null still has its scope at the
    // authored tab position for a later swap to build under. Placement is a
    // non-destructive move (addChild), so it never clears focus.
    registerFocusScope(parentFM, fallbackParent, placeScope);

    auto* nestedInstance = artboardInstance(0);
    if (nestedInstance == nullptr)
    {
        return;
    }

    // Skip the destructive teardown+rebuild when the subtree already shares
    // this manager and was not just re-wired (forceRebuild): rebuilding an
    // untouched nested instance clears focus resting inside it, while the scope
    // placement above already keeps tab order correct. forceRebuild is set
    // after setExternalFocusManager (which rebuilds at the manager root) or on
    // a swap.
    if (!forceRebuild && nestedInstance->focusManager() == parentFM)
    {
        return;
    }

    nestedInstance->cleanupFocusTree();
    // Under the scope when data-bound, else under fallbackParent (the caller's
    // resolved closest ancestor).
    nestedInstance->buildFocusTree(parentFM,
                                   m_focusScope != nullptr ? m_focusScope
                                                           : fallbackParent);
}

static Mat2D makeTranslate(const Artboard* artboard)
{
    return Mat2D::fromTranslate(-artboard->originX() * artboard->width(),
                                -artboard->originY() * artboard->height());
}

void NestedArtboard::draw(Renderer* renderer)
{
    if (m_needsSaveOperation)
    {
        renderer->save();
    }
    renderer->transform(worldTransform());
    m_referencedArtboard->drawInternal(renderer);
    if (m_needsSaveOperation)
    {
        renderer->restore();
    }
}

bool NestedArtboard::willDraw()
{
    return Super::willDraw() && m_referencedArtboard != nullptr;
}

Core* NestedArtboard::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    if (m_referencedArtboard == nullptr)
    {
        return nullptr;
    }
    hinfo->mounts.push_back(this);
    auto mx = xform * worldTransform() * makeTranslate(m_referencedArtboard);
    if (auto c = m_referencedArtboard->hitTest(hinfo, mx))
    {
        return c;
    }
    hinfo->mounts.pop_back();
    return nullptr;
}

bool NestedArtboard::hitTestHost(const Vec2D& position,
                                 bool skipOnUnclipped,
                                 ArtboardInstance* artboard)
{
    return parent()->hitTestPoint(worldTransform() * position,
                                  skipOnUnclipped,
                                  false);
}

Vec2D NestedArtboard::hostTransformPoint(const Vec2D& vec,
                                         ArtboardInstance* artboardInstance)
{
    auto localVec = Vec2D::transformMat2D(vec, worldTransform());
    auto ab = artboard();
    return ab ? ab->rootTransform(localVec) : localVec;
}

Mat2D NestedArtboard::worldTransformForArtboard(ArtboardInstance*)
{
    return worldTransform();
}

StatusCode NestedArtboard::import(ImportStack& importStack)
{
    importDataBindPath(importStack);
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addArtboardReferencer(this);

    return Super::import(importStack);
}

void NestedArtboard::addNestedAnimation(NestedAnimation* nestedAnimation)
{
    m_NestedAnimations.push_back(nestedAnimation);
}

StatusCode NestedArtboard::onAddedClean(CoreContext* context)
{
    // N.B. The nested instance will be null here for the source artboards.
    // Instances will have a nestedInstance available. This is a good thing as
    // it ensures that we only instance animations in artboard instances. It
    // does require that we always use an artboard instance (not just the source
    // artboard) when working with nested artboards, but in general this is good
    // practice for any loaded Rive file.
    assert(m_referencedArtboard == nullptr ||
           m_referencedArtboard == m_Instance.get());

    if (m_Instance)
    {
        for (auto animation : m_NestedAnimations)
        {
            animation->initializeAnimation(m_Instance.get());
        }
        m_referencedArtboard->host(this);
        // Children are linked by now, so an authored origin override child is
        // resolvable; push it onto the mounted instance.
        applyOriginOverride();
    }

    // ViewModelInstance children belong to NestedArtboards that wrap a
    // stateful component Artboard. A single "standard" VMI becomes the active
    // local-root binding (borrowed; the parent Artboard's m_Objects teardown
    // owns its lifetime), and any number of "global" VMIs are appended to the
    // data context passed down to the wrapped artboard.
    for (auto child : children())
    {
        if (!child->is<ViewModelInstance>())
        {
            continue;
        }
        auto vmi = child->as<ViewModelInstance>();
        auto vm = vmi->viewModel();
        auto type = vm != nullptr
                        ? static_cast<ViewModelType>(vm->viewModelType())
                        : ViewModelType::standard;
        m_file->completeViewModelProperties(vmi);
        if (type == ViewModelType::global)
        {
            // The artboard's m_Objects owns the construction ref; take an
            // extra ref so the list outlives any teardown ordering.
            m_globalViewModelInstances.push_back(ref_rcp(vmi));
        }
        else if (m_activeViewModelInstance == nullptr)
        {
            // Borrow the first standard child VMI as the active binding.
            m_activeViewModelInstance = vmi;
            m_ownsActiveVmi = false;
        }
    }
    tryScheduleBindStateful();
    detectArtboardDataBinding();

    return Super::onAddedClean(context);
}

void NestedArtboard::update(ComponentDirt value)
{
    Super::update(value);
    if (m_referencedArtboard == nullptr)
    {
        return;
    }
    if (hasDirt(value, ComponentDirt::WorldTransform))
    {
        // Mark semantic bounds dirty for nodes inside the nested artboard.
        // Their root-space bounds depend on the host's world transform.
        if (m_Instance != nullptr)
        {
            m_Instance->markSemanticBoundaryTransformDirty();
        }
    }
    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        m_referencedArtboard->opacity(renderOpacity());
    }
    if (hasDirt(value, ComponentDirt::Components))
    {
        // We intentionally discard whether or not this updated because by the
        // end of the pass all the dirt is removed and only another advance of
        // animations/statemachines can re-add it.
        m_referencedArtboard->updatePass(false);
    }
}

bool NestedArtboard::collapse(bool value)
{
    if (!Super::collapse(value))
    {
        return false;
    }

    auto* nestedInstance = artboardInstance();
    if (nestedInstance == nullptr)
    {
        return true;
    }
    // Semantic-only collapse via the artboard boundary node. Only touches
    // SemanticData nodes — non-semantic components stay untouched.
    nestedInstance->collapseSemanticBoundary(value);
    return true;
}

bool NestedArtboard::hasNestedStateMachines() const
{
    for (auto animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>())
        {
            return true;
        }
    }
    return false;
}

Span<NestedAnimation*> NestedArtboard::nestedAnimations()
{
    return m_NestedAnimations;
}

NestedArtboard* NestedArtboard::nestedArtboard(std::string name) const
{
    if (m_Instance != nullptr)
    {
        return m_Instance->nestedArtboard(name);
    }
    return nullptr;
}

NestedStateMachine* NestedArtboard::stateMachine(std::string name) const
{
    for (auto animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>() && animation->name() == name)
        {
            return animation->as<NestedStateMachine>();
        }
    }
    return nullptr;
}

NestedInput* NestedArtboard::input(std::string name) const
{
    return input(name, "");
}

NestedInput* NestedArtboard::input(std::string name,
                                   std::string stateMachineName) const
{
    if (!stateMachineName.empty())
    {
        auto nestedSM = stateMachine(stateMachineName);
        if (nestedSM != nullptr)
        {
            return nestedSM->input(name);
        }
    }
    else
    {
        for (auto animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                auto input = animation->as<NestedStateMachine>()->input(name);
                if (input != nullptr)
                {
                    return input;
                }
            }
        }
    }
    return nullptr;
}

bool NestedArtboard::worldToLocal(Vec2D world, Vec2D* local)
{
    assert(local != nullptr);
    if (m_referencedArtboard == nullptr)
    {
        return false;
    }
    Mat2D toMountedArtboard;
    if (!worldTransform().invert(&toMountedArtboard))
    {
        return false;
    }

    *local = toMountedArtboard * world;

    return true;
}

Vec2D NestedArtboard::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    return Vec2D(std::min(widthMode == LayoutMeasureMode::undefined
                              ? std::numeric_limits<float>::max()
                              : width,
                          m_Instance ? m_Instance->width() : 0.0f),
                 std::min(heightMode == LayoutMeasureMode::undefined
                              ? std::numeric_limits<float>::max()
                              : height,
                          m_Instance ? m_Instance->height() : 0.0f));
}

void NestedArtboard::controlSize(Vec2D size,
                                 LayoutScaleType widthScaleType,
                                 LayoutScaleType heightScaleType,
                                 LayoutDirection direction)
{}

void NestedArtboard::decodeDataBindPathIds(Span<const uint8_t> value)
{
    decodeDataBindPath(value);
}

void NestedArtboard::copyDataBindPathIds(const NestedArtboardBase& object)
{
    copyDataBindPath(object.as<NestedArtboard>()->dataBindPath());
}

void NestedArtboard::internalDataContext(rcp<DataContext> value)
{
    m_dataContext = value;
    m_viewModelInstance = nullptr;

    if (artboardInstance() == nullptr)
    {
        return;
    }

    // If we have a stateful ViewModelInstance, bind it (with globals) via
    // the scheduled stateful path.
    if (tryScheduleBindStateful())
    {
        return;
    }

    if (!m_globalViewModelInstances.empty())
    {
        auto list = buildVMIList(nullptr, m_globalViewModelInstances);
        artboardInstance()->bindViewModelInstances(std::move(list), value);
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->dataContext(
                    artboardInstance()->dataContext());
            }
        }
        return;
    }

    // Non-stateful path with no globals: just propagate the data context.
    artboardInstance()->internalDataContext(value);
    for (auto& animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>())
        {
            animation->as<NestedStateMachine>()->dataContext(value);
        }
    }
}

void NestedArtboard::relinkDataContext(rcp<ViewModelInstance> viewModelInstance)
{
    m_viewModelInstance = viewModelInstance;
    auto instance = artboardInstance(0);
    if (instance && !isStateful())
    {
        auto dataContext = instance->dataContext();
        if (dataContext != nullptr)
        {
            if (dataContext->mainViewModelInstance() != viewModelInstance)
            {
                dataContext->viewModelInstance(viewModelInstance);
            }
        }
        instance->relinkDataContext();
    }
}

void NestedArtboard::clearDataContext()
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->clearDataContext();
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->clearDataContext();
            }
        }
    }
}

void NestedArtboard::unbind()
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->unbind();
    }
}

void NestedArtboard::updateDataBinds()
{
    if (artboardInstance() != nullptr && !isPaused())
    {
        artboardInstance()->updateDataBinds();
    }
}

void NestedArtboard::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance,
    rcp<DataContext> parent)
{
    m_dataContext = parent;
    m_viewModelInstance = viewModelInstance;
    if (artboardInstance() == nullptr)
    {
        return;
    }

    // Stateful: local root is the active VMI; append nested globals.
    if (m_activeViewModelInstance != nullptr)
    {
        auto list = buildVMIList(ref_rcp(m_activeViewModelInstance),
                                 m_globalViewModelInstances);
        artboardInstance()->bindViewModelInstances(std::move(list), parent);
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->dataContext(
                    artboardInstance()->dataContext());
            }
        }
        return;
    }

    // Non-stateful with a passed VMI or nested globals: build composite DC.
    if (viewModelInstance != nullptr || !m_globalViewModelInstances.empty())
    {
        auto list = buildVMIList(viewModelInstance, m_globalViewModelInstances);
        artboardInstance()->bindViewModelInstances(std::move(list), parent);
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->dataContext(
                    artboardInstance()->dataContext());
            }
        }
        return;
    }

    // Nothing to merge in — propagate parent context unchanged (legacy
    // passthrough). Binding a null instance would call
    // Artboard::bindViewModelInstance(nullptr, ...), which now triggers
    // unbind(); use internalDataContext to keep the parent context wired.
    artboardInstance()->internalDataContext(parent);
    for (auto& animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>())
        {
            animation->as<NestedStateMachine>()->dataContext(parent);
        }
    }
}

float NestedArtboard::calculateLocalElapsedSeconds(float elapsedSeconds)
{
    auto localElapsedSeconds = elapsedSeconds * (speed() >= 0 ? speed() : 1);
    if (quantize() >= 0)
    {
        m_cumulatedSeconds += localElapsedSeconds;
        auto quantizedSeconds = 1 / quantize();
        if (m_cumulatedSeconds > quantizedSeconds)
        {
            localElapsedSeconds =
                std::floor(m_cumulatedSeconds / quantizedSeconds) *
                quantizedSeconds;
            m_cumulatedSeconds -= localElapsedSeconds;
        }
        else
        {
            localElapsedSeconds = 0;
        }
    }
    return localElapsedSeconds;
}

bool NestedArtboard::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
    if (m_referencedArtboard == nullptr || isCollapsed() || isPaused())
    {
        return false;
    }
    if (enums::is_flag_set(m_hostFlags,
                           NestedArtboardHostFlags::pendingStatefulBinding))
    {
        bindStateful();
    }
    bool keepGoing = false;
    bool advanceNested =
        (flags & AdvanceFlags::AdvanceNested) == AdvanceFlags::AdvanceNested;
    auto localElapsedSeconds = calculateLocalElapsedSeconds(elapsedSeconds);
    bool newFrame = (flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame;
    // If the elapsed time is 0 because of quantization, we still want to
    // continue advancing until the cumulated time is flushed
    if (localElapsedSeconds == 0 && quantize() >= 0 && newFrame)
    {
        return true;
    }
    if (advanceNested)
    {
        for (auto animation : m_NestedAnimations)
        {
            // If it is not a new frame, we only advance state machines. And we
            // first validate whether their state has changed. Then and only
            // then we advance the state machine. This avoids triggering dirt
            // from advances that make intermediate value changes but finally
            // settle in the same value
            if (!newFrame)
            {
                if (animation->is<NestedStateMachine>())
                {
                    if (animation->as<NestedStateMachine>()->tryChangeState())
                    {
                        if (animation->advance(localElapsedSeconds, newFrame))
                        {
                            keepGoing = true;
                        }
                    }
                }
            }
            else
            {

                if (animation->advance(localElapsedSeconds, newFrame))
                {
                    keepGoing = true;
                }
            }
        }
    }

    auto advancingFlags = flags & ~AdvanceFlags::IsRoot;
    if (m_referencedArtboard->advanceInternal(localElapsedSeconds,
                                              advancingFlags))
    {
        keepGoing = true;
    }
    if (m_referencedArtboard->hasDirt(ComponentDirt::Components))
    {
        // The animation(s) caused the artboard to need an update.
        addDirt(ComponentDirt::Components);
    }

    return keepGoing;
}

void NestedArtboard::reset()
{
    if (m_referencedArtboard)
    {
        m_referencedArtboard->reset();
    }
    if (m_activeViewModelInstance != nullptr)
    {
        m_activeViewModelInstance->advanced();
    }
}

void NestedArtboard::file(File* value) { m_file = value; }

File* NestedArtboard::file() const { return m_file; }

int NestedArtboard::referencedArtboardId() { return artboardId(); }

void NestedArtboard::referencedArtboard(Artboard* artboard)
{
    assert(artboard != nullptr);
    ArtboardReferencer::referencedArtboard(artboard);
    nest(artboard);
    tryScheduleBindStateful();
}