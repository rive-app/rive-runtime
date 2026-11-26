#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_mesh_host.hpp"
#include "rive/component_origin.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/backboard.hpp"
#include "rive/file.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/focus_data.hpp"
#include "rive/layer_mask.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/focusable.hpp"
#include "rive/nested_animation.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/nested_remap_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive/semantic/semantic_manager.hpp"
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
  NestedArtboard* nestedArtboard = static_cast<NestedArtboard*>(
      NestedArtboardBase::clone());
  // Carry the swap-slot flag to clones. It is detected on the SOURCE
  // artboard's host (import populates the source's data binds before
  // onAddedClean)
  if (isArtboardDataBound())
  {
    nestedArtboard->m_hostFlags |= NestedArtboardHostFlags::artboardDataBound;
  }
  cloneReferencesInto(nestedArtboard);
  return nestedArtboard;
}

void NestedArtboard::cloneReferencesInto(NestedArtboard* clone) const
{
  clone->file(file());
  // A collapsed host's clone mounts the first time it is shown, when its
  // file defers them. One bound to an artboard property mounts whatever the
  // binding names, so the binding is left to decide. One that has mounted
  // (cloning an instance) clones what it mounted, which may have changed
  // since: mounting from the source later would lose that.
  if (m_file != nullptr && m_file->defersCollapsedNestedArtboards() &&
      !isArtboardDataBound() && m_mountedBindable == nullptr &&
      (isInstanceDeferred() ||
       (m_referencedArtboard != nullptr &&
        !m_referencedArtboard->isInstance() && isCollapsed())))
  {
    clone->m_hostFlags |= NestedArtboardHostFlags::deferredInstance |
                          NestedArtboardHostFlags::heldInputs;
    return;
  }
  if (m_referencedArtboard == nullptr)
  {
    return;
  }
  // A clone of a mounted bindable's instance needs that bindable's file as
  // much as the instance does, and binds the same view model instance.
  clone->m_mountedBindable = m_mountedBindable;
  clone->m_ownViewModelInstance = m_ownViewModelInstance;
  clone->referencedArtboard(m_referencedArtboard->instance().release());
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
  m_referencedArtboard->hostOpacity(renderOpacity());
  m_referencedArtboard->volume(artboard->volume());
  m_Instance = nullptr;
  // A new instance isn't marked quiet yet.
  m_hostFlags &= ~NestedArtboardHostFlags::inactiveMarked;
  if (m_meshHost != nullptr)
  {
    m_meshHost->resetRaster();
  }
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
  // A row that went quiet with no instance here, or another one, has this
  // one to advance.
  wakeRow();
  // Everything this host draws was just replaced. Told outright, because a
  // fresh instance of the same artboard can carry the same change serial as
  // the one it replaces, and syncLayerMasks would see nothing to report.
  m_layerMaskSerial = m_referencedArtboard->changeSerial();
  invalidateLayerMasks();
}

// A layer mask caches the pixels of what it masks and of its source, and
// learns they are stale from dirt reaching it through its artboard's
// dependency graph. What this host draws changes in another artboard: that
// dirt is raised in the instance's own graph, and advanceComponent lifts
// Components onto this host without recursing, so a mask over or sourced from
// a nested artboard kept compositing its first raster. The instance's change
// serial counts every change that reaches it, a dirt-free recolour included,
// so a new one is exactly "these pixels may have moved".
void NestedArtboard::syncLayerMasks()
{
  if (m_referencedArtboard == nullptr)
  {
    return;
  }
  const uint32_t serial = m_referencedArtboard->changeSerial();
  if (serial == m_layerMaskSerial)
  {
    return;
  }
  m_layerMaskSerial = serial;
  invalidateLayerMasks();
}

// Told directly, as SolidColor::colorValueChanged tells them: there is no
// spare dirt bit, and cascading one from here would reach every other
// dependent of this host too.
void NestedArtboard::invalidateLayerMasks()
{
  for (LayerMask* mask : layerMasks())
  {
    mask->markRasterDirty();
  }
  for (LayerMask* mask : sourceOfLayerMasks())
  {
    mask->markRasterDirty();
  }
}

void NestedArtboard::applyOriginOverride()
{
  if (m_referencedArtboard == nullptr || !m_referencedArtboard->isInstance())
  {
    return;
  }
  for (auto child : children())
  {
    if (child->is<ComponentOrigin>())
    {
      auto origin = child->as<ComponentOrigin>();
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
  auto list = buildVMIList(
      ref_rcp(m_activeViewModelInstance),
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

void NestedArtboard::setActiveViewModelInstance(
    ViewModelInstance* vmi,
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
  swapArtboard(viewModelInstanceArtboard, true);
}

void NestedArtboard::swapArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard,
    bool mount)
{
  // A VM artboard swap means this host is data-bound.
  m_hostFlags |= NestedArtboardHostFlags::artboardDataBound;

  // Resolve the swap target BEFORE tearing anything down. An empty value is
  // the user explicitly clearing the artboard (a real "set to null"); any
  // OTHER unresolvable id must leave the currently displayed artboard — and
  // its focus tree — untouched, rather than blanking focus for content that
  // stays on screen. findArtboard has no side effects, so resolving it early
  // is safe.
  bool explicitNull = viewModelInstanceArtboard != nullptr &&
                      viewModelInstanceArtboard->isEmpty();
  Artboard* artboard = explicitNull ? nullptr
                                    : findArtboard(
                                          viewModelInstanceArtboard,
                                          parentArtboard(),
                                          m_file);
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
    outgoing->cleanupSemanticTree();
  }
  clearDataContext();
  clearNestedAnimations();
  m_boundNestedStateMachine = nullptr;

  if (explicitNull || !mount)
  {
    // Only detach a non-owned reference; an owned instance must be
    // destroyed with m_host intact so ~LayoutComponent can clean the
    // hosting artboard's dirty layout set.
    if (m_referencedArtboard != nullptr && m_Instance == nullptr)
    {
      m_referencedArtboard->host(nullptr);
    }
    m_referencedArtboard = nullptr;
    m_Instance = nullptr;
    m_mountedBindable = nullptr;
    m_ownViewModelInstance = nullptr;
    if (m_meshHost != nullptr)
    {
      m_meshHost->resetRaster();
    }
    // Nothing mounted means nothing to sync from again, so the masks hear
    // about the empty host now or never.
    invalidateLayerMasks();
    // A swap leaves the stateful instance to the next mount.
    if (explicitNull)
    {
      setActiveViewModelInstance(nullptr, false);
    }
    return;
  }

  if (artboard != nullptr)
  {
    // The host's factory so a databound swap, possibly from another
    // file, keeps the nested content on the hosting instance's session.
    auto artboardInstance = artboard->instance<ArtboardInstance>(
        this->artboard() != nullptr ? this->artboard()->factory() : nullptr);
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
    // The outgoing instance was destroyed by the swap above, so the
    // bindable it came from can go now.
    m_mountedBindable = viewModelInstanceArtboard->asset();

    // An artboard from another file binds to a view model of that file,
    // which its bindable carries. This host's stateful instances are of
    // this file's view models: comparing ids across files, or looking
    // one up here, would bind it to the wrong properties.
    if (isStateful() && foreignFile() == nullptr)
    {
      auto statefulChild = findStatefulChildVmi();
      if (statefulChild != nullptr &&
          statefulChild->viewModelId() == artboard->viewModelId())
      {
        // Source artboard uses the same VM as the stateful child:
        // borrow the child instance.
        setActiveViewModelInstance(statefulChild, false);
      }
      else if (
          m_ownsActiveVmi && m_activeViewModelInstance != nullptr &&
          m_activeViewModelInstance->viewModelId() == artboard->viewModelId())
      {
        // Already holding a bound instance for this VM; reuse.
      }
      else
      {
        auto vm = m_file != nullptr ? m_file->viewModel(artboard->viewModelId())
                                    : nullptr;
        rcp<ViewModelInstance>
            newBound = vm != nullptr
                           ? m_file->createDefaultViewModelInstance(vm)
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
      // Mounting scheduled a stateful bind for the outgoing instance;
      // with none active it would unbind the new one on next advance.
      m_hostFlags &= ~NestedArtboardHostFlags::pendingStatefulBinding;
    }

    m_ownViewModelInstance = viewModelInstanceArtboard
                                 ->boundViewModelInstance();
    if (m_ownViewModelInstance != nullptr)
    {
      bindViewModelInstance(m_viewModelInstance, m_dataContext);
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
    auto* parentFM = parentAb != nullptr ? parentAb->focusManager() : nullptr;
    if (parentFM != nullptr && m_boundNestedStateMachine != nullptr)
    {
      auto* smi = m_boundNestedStateMachine->stateMachineInstance();
      if (smi != nullptr && smi->focusManager() != parentFM)
      {
        smi->setExternalFocusManager(parentFM, /*buildTree=*/false);
      }
    }
    // Build the nested instance's focus tree under the host's scope. The
    // wiring above left it unbuilt, so this is its one build.
    syncNestedFocusTree(FocusData::findClosestFocusNode(this));

    // Register the swapped-in instance with the host's semantic manager,
    // reparented under the closest SemanticData in this artboard, or its
    // boundary, the same way
    // ArtboardComponentList wires its items
    auto* parentSemanticManager = parentAb != nullptr
                                      ? parentAb->semanticManager()
                                      : nullptr;
    if (parentSemanticManager != nullptr)
    {
      auto parentSemanticNode = SemanticData::findClosestSemanticNode(this);
      auto* semanticSmi = m_boundNestedStateMachine != nullptr
                              ? m_boundNestedStateMachine
                                    ->stateMachineInstance()
                              : nullptr;
      if (semanticSmi != nullptr)
      {
        semanticSmi->setExternalSemanticManager(
            parentSemanticManager,
            parentSemanticNode);
      }
      else if (m_Instance != nullptr)
      {
        // Artboards with no state machine have no instance to hang
        // the external manager off; build the subtree directly.
        m_Instance->buildSemanticTree(
            parentSemanticManager,
            parentSemanticNode);
      }
    }
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
          dataBind->propertyKey() == NestedArtboardBase::artboardIdPropertyKey)
      {
        m_hostFlags |= NestedArtboardHostFlags::artboardDataBound;
        return;
      }
    }
  }
}

void NestedArtboard::registerFocusScope(
    FocusManager* focusManager,
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

void NestedArtboard::syncNestedFocusTree(
    rcp<FocusNode> fallbackParent,
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
  // after setExternalFocusManager (which leaves the tree unbuilt, or built at
  // the manager root) or on a swap.
  if (!forceRebuild && nestedInstance->focusManager() == parentFM)
  {
    return;
  }

  // A deferred tree was torn down when the manager was wired and has not
  // been built since: there is nothing to clean up.
  if (!nestedInstance->focusTreeDeferred())
  {
    nestedInstance->cleanupFocusTree();
  }
  // Under the scope when data-bound, else under fallbackParent (the caller's
  // resolved closest ancestor).
  nestedInstance->buildFocusTree(
      parentFM,
      m_focusScope != nullptr ? m_focusScope : fallbackParent);
}

static Mat2D makeTranslate(const Artboard* artboard)
{
  return Mat2D::fromTranslate(
      -artboard->originX() * artboard->width(),
      -artboard->originY() * artboard->height());
}

NestedArtboardMeshHost* NestedArtboard::meshHost()
{
  if (m_meshHost == nullptr)
  {
    m_meshHost = std::make_unique<NestedArtboardMeshHost>(this);
  }
  return m_meshHost.get();
}

MeshDrawable* NestedArtboard::hostedMesh() const
{
  return m_meshHost != nullptr ? m_meshHost->hostedMesh() : nullptr;
}

void NestedArtboard::draw(Renderer* renderer)
{
  bool bypassCache = false;
#ifdef RIVE_CANVAS
  // A visitor needs drawables to visit, so it gets the undeformed vectors,
  // as with Bitmap Cache.
  if (hostedMesh() != nullptr && !artboard()->hasDrawVisitor())
  {
    // Any composite would drop the clip, the source's own cache included:
    // draw clipped vectors.
    bypassCache = m_meshHost->compositeDropsClip();
    // Pointers follow whichever way this actually draws.
    const bool drewThroughMesh = !bypassCache && m_meshHost->draw(renderer);
    m_meshHost->drewThroughMesh(drewThroughMesh);
    if (drewThroughMesh)
    {
      return;
    }
  }
#endif
  if (needsSaveOperation())
  {
    renderer->save();
  }
  renderer->transform(worldTransform());
  artboard()->drawHosted(m_referencedArtboard, renderer, bypassCache);
  if (needsSaveOperation())
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

bool NestedArtboard::hitTestHost(
    const Vec2D& position,
    bool skipOnUnclipped,
    ArtboardInstance* artboard)
{
  Vec2D world;
  if (m_meshHost == nullptr || !m_meshHost->worldThroughMesh(position, &world))
  {
    world = worldTransform() * position;
  }
  return parent()->hitTestPoint(world, skipOnUnclipped, false);
}

Vec2D NestedArtboard::hostTransformPoint(
    const Vec2D& vec,
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
  auto backboardImporter = importStack.latest<BackboardImporter>(
      Backboard::typeKey);
  if (backboardImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }
  StatusCode code = Super::import(importStack);
  if (code != StatusCode::Ok)
  {
    return code;
  }
  // Registered last, and only once this object is certain to survive.
  backboardImporter->addArtboardReferencer(this);
  return StatusCode::Ok;
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
  assert(
      m_referencedArtboard == nullptr ||
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
    auto type = vm != nullptr ? static_cast<ViewModelType>(vm->viewModelType())
                              : ViewModelType::standard;
    // Editor-native documents own coop arenas rather than a runtime File.
    // Runtime imports still complete missing properties through their File.
    if (m_file != nullptr)
    {
      m_file->completeViewModelProperties(vmi);
    }
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
    m_referencedArtboard->hostOpacity(renderOpacity());
  }
  // Also flush when the referenced artboard itself is dirty (not just the
  // host). The RenderOpacity branch above writes the host opacity onto the
  // referenced artboard, which marks its own Components dirt after
  // advanceComponent has already run. Left for the next advance to lift, the
  // children would draw the old opacity for a frame (forever when paused,
  // since advanceComponent short-circuits). A host that settles in a loop
  // hides that; one advanced once per frame, as a script's Artboard:advance
  // is, does not. updatePass recomputes render state from the current values
  // without advancing animations, preserving pause.
  if (hasDirt(value, ComponentDirt::Components) ||
      m_referencedArtboard->hasDirt(ComponentDirt::Components))
  {
    // We intentionally discard whether or not this updated because by the
    // end of the pass all the dirt is removed and only another advance of
    // animations/statemachines can re-add it.
    m_referencedArtboard->updatePass(false);
  }
  // The pass above applies data binds and host opacity, after
  // advanceComponent last looked.
  syncLayerMasks();
}

bool NestedArtboard::collapse(bool value)
{
  if (!Super::collapse(value))
  {
    return false;
  }
  if (!value)
  {
    mountDeferred();
  }

  auto* nestedInstance = artboardInstance();
  if (nestedInstance == nullptr)
  {
    return true;
  }
  // The hosted artboard's semantics are hidden with its host; its own
  // components are not collapsed.
  nestedInstance->syncSemanticBoundary();
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
  // Looked up by name, as by path (Artboard::nestedArtboardAtPath): a
  // deferred host mounts what the caller reaches into.
  const_cast<NestedArtboard*>(this)->mountDeferred();
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

NestedInput* NestedArtboard::input(
    std::string name,
    std::string stateMachineName) const
{
  // A nested input's name is its machine's input's (NestedInput::name), so
  // a deferred host mounts for the lookup.
  const_cast<NestedArtboard*>(this)->mountDeferred();
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

bool NestedArtboard::worldToLocal(
    Vec2D world,
    Vec2D* local,
    bool* offMesh,
    Mat2D* toLocal)
{
  assert(local != nullptr);
  if (m_referencedArtboard == nullptr)
  {
    return false;
  }
  bool pastMesh = false;
  if (m_meshHost != nullptr && m_meshHost->pointersFollowMesh())
  {
    if (m_meshHost->localThroughMesh(world, local, &pastMesh, toLocal))
    {
      if (offMesh != nullptr)
      {
        *offMesh = pastMesh;
      }
      return true;
    }
    // Drawn through a mesh with no triangles to map through, such as an
    // N-Slice with every patch hidden: nothing is drawn to hit. The
    // undeformed position below still keeps a drag moving.
    pastMesh = true;
  }
  if (offMesh != nullptr)
  {
    *offMesh = pastMesh;
  }
  Mat2D toMountedArtboard;
  if (!worldTransform().invert(&toMountedArtboard))
  {
    return false;
  }

  *local = toMountedArtboard * world;
  if (toLocal != nullptr)
  {
    *toLocal = toMountedArtboard;
  }

  return true;
}

Vec2D NestedArtboard::measureLayout(
    float width,
    LayoutMeasureMode widthMode,
    float height,
    LayoutMeasureMode heightMode)
{
  return Vec2D(
      std::min(
          widthMode == LayoutMeasureMode::undefined
              ? std::numeric_limits<float>::max()
              : width,
          m_Instance ? m_Instance->width() : 0.0f),
      std::min(
          heightMode == LayoutMeasureMode::undefined
              ? std::numeric_limits<float>::max()
              : height,
          m_Instance ? m_Instance->height() : 0.0f));
}

void NestedArtboard::controlSize(
    Vec2D size,
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

  if (m_ownViewModelInstance != nullptr)
  {
    bindViewModelInstance(nullptr, value);
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
    if (dataContext != nullptr && m_ownViewModelInstance == nullptr)
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
  else
  {
    forgetDeferredDataContext();
  }
}

void NestedArtboard::unbind()
{
  if (artboardInstance() != nullptr)
  {
    artboardInstance()->unbind();
  }
  else
  {
    forgetDeferredDataContext();
  }
}

void NestedArtboard::forgetDeferredDataContext()
{
  if (!isInstanceDeferred())
  {
    return;
  }
  // Nothing is bound yet, but mounting binds the context this host was
  // handed: let it go, or mounting rebinds what the caller cleared. A bind
  // that follows hands it a new one.
  m_dataContext = nullptr;
  m_viewModelInstance = nullptr;
}

void NestedArtboard::updateDataBinds()
{
  if (artboardInstance() == nullptr || isPaused() || isCollapsed())
  {
    return;
  }
  // Nothing queued in the instance or anything it hosts: walking it would
  // only find empty queues.
  if (!artboardInstance()->hasBindWorkBelow())
  {
#ifdef TESTING
    artboardInstance()->verifyNoBindWorkBelow();
    sm_bindWalkSkips++;
#endif
    return;
  }
  artboardInstance()->updateDataBinds();
}

bool NestedArtboard::hasBindWork()
{
  auto instance = artboardInstance();
  return instance != nullptr && instance->hasBindWorkBelow();
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
    auto list = buildVMIList(
        ref_rcp(m_activeViewModelInstance),
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

  if (m_ownViewModelInstance != nullptr)
  {
    viewModelInstance = m_ownViewModelInstance;
    if (foreignFile() != nullptr)
    {
      // Its file's view model ids and root are not the host's.
      parent = nullptr;
    }
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
      localElapsedSeconds = std::floor(m_cumulatedSeconds / quantizedSeconds) *
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

namespace
{
// Where a settle pass (an advance that is not a new frame) applies a host's
// animations again: from the first remap whose time changed since it last
// applied (a data bind, a joystick or a host's animation wrote it after this
// frame's advance) to the last animation, in order, so a later one still wins
// where they key the same properties. Applying one again has to give what this
// frame's advance gave, so each of those must be a remap or simple animation
// at mix 0 or 1: at another mix it would blend in a second time, and a state
// machine can't apply without advancing. A remap before one that can't waits
// for the next frame's advance. animations.size() when none applies.
size_t remapReapplyStart(const std::vector<NestedAnimation*>& animations)
{
  size_t count = animations.size();
  size_t start = count;
  for (size_t i = 0; i < count; i++)
  {
    NestedAnimation* animation = animations[i];
    bool reappliesAsIs = false;
    if (animation->is<NestedLinearAnimation>())
    {
      float mix = animation->as<NestedLinearAnimation>()->mix();
      reappliesAsIs = mix == 0.0f || mix == 1.0f;
    }
    if (!reappliesAsIs)
    {
      start = count;
    }
    else if (
        start == count && animation->is<NestedRemapAnimation>() &&
        animation->as<NestedRemapAnimation>()->needsApply())
    {
      start = i;
    }
  }
  return start;
}
} // namespace

bool NestedArtboard::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
  if (m_referencedArtboard == nullptr)
  {
    // Shown without collapse(false) reaching it (nothing collapsed this
    // instance's host though its source host was): it mounts now.
    if (!isInstanceDeferred() || isCollapsed())
    {
      return false;
    }
    mountDeferred();
    if (m_referencedArtboard == nullptr)
    {
      return false;
    }
  }
  if (isCollapsed() || isPaused())
  {
#ifdef TESTING
    // Standing in for a skipped pass, which notes nothing.
    if (ArtboardComponentList::sm_verifyingQuietRow)
    {
      return false;
    }
#endif
    m_inactiveChangeSequence = ViewModelInstanceValue::latestChangeSequence();
    // A paused instance still draws, and a write into it that raises no
    // dirt, like a recolour, shows without advancing it.
    syncLayerMasks();
    if (isCollapsed() && m_Instance != nullptr &&
        !enums::is_flag_set(
            m_hostFlags,
            NestedArtboardHostFlags::inactiveMarked))
    {
      // From here, a change the machines inside read wakes this host
      // (hostedRowWoke), so until one does, the advances after this one
      // can be skipped (see quietState).
      m_Instance->quietHostRow(0);
      m_Instance->markHostedQuiet();
      m_hostFlags |= NestedArtboardHostFlags::inactiveMarked;
    }
    return false;
  }
  m_hostFlags &= ~NestedArtboardHostFlags::inactiveMarked;
  if (enums::is_flag_set(
          m_hostFlags,
          NestedArtboardHostFlags::pendingStatefulBinding))
  {
    bindStateful();
  }
  bool keepGoing = false;
  bool advanceNested = (flags & AdvanceFlags::AdvanceNested) ==
                       AdvanceFlags::AdvanceNested;
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
    size_t animationCount = m_NestedAnimations.size();
    size_t reapplyFrom = newFrame ? animationCount
                                  : remapReapplyStart(m_NestedAnimations);
    for (size_t i = 0; i < animationCount; i++)
    {
      NestedAnimation* animation = m_NestedAnimations[i];
      // If it is not a new frame, we only advance state machines. And we
      // first validate whether their state has changed. Then and only
      // then we advance the state machine. This avoids triggering dirt
      // from advances that make intermediate value changes but finally
      // settle in the same value
      if (!newFrame)
      {
        // A remap time written after this frame's advance shows this frame:
        // it and the animations after it apply again, in order, without
        // moving a clock (see remapReapplyStart). Their applies dirty the
        // instance, which the check below carries to this host.
        if (i >= reapplyFrom)
        {
          if (animation->is<NestedRemapAnimation>())
          {
            // Clears needsApply.
            animation->advance(localElapsedSeconds, newFrame);
          }
          else
          {
            animation->as<NestedLinearAnimation>()->applyPose();
          }
        }
        else if (animation->is<NestedStateMachine>())
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
  if (m_referencedArtboard->advanceInternal(
          localElapsedSeconds,
          advancingFlags))
  {
    keepGoing = true;
  }
  if (m_referencedArtboard->hasDirt(ComponentDirt::Components))
  {
    // The animation(s) caused the artboard to need an update.
    addDirt(ComponentDirt::Components);
  }
  // Here and not only in update(): a recolour raises no dirt, so nothing
  // would schedule that.
  syncLayerMasks();

  return keepGoing;
}

AdvancingComponent::QuietState NestedArtboard::quietState()
{
  using QuietState = AdvancingComponent::QuietState;
  if (m_referencedArtboard == nullptr)
  {
    // A deferred host that is shown mounts on its next advance.
    return isInstanceDeferred() && !isCollapsed() ? QuietState::busy
                                                  : QuietState::quiet;
  }
  // An instance this host doesn't own may be advanced elsewhere too.
  if (m_Instance == nullptr)
  {
    return QuietState::never;
  }
  // advanceComponent's early out. A collapsed or paused host only notes
  // where the change sequence stands, for the state machines inside once
  // they run again. A change made after that is still pending for them.
  // A paused host stays busy.
  if (isPaused())
  {
    return QuietState::busy;
  }
  // A collapsed one is quiet once an advance has marked what it hosts, as
  // long as nothing inside needs the advance's note
  // (skippableWhileCollapsed). A change a list row inside reads still wakes
  // it, and its next advance notes the change.
  if (isCollapsed())
  {
    bool marked = enums::is_flag_set(
        m_hostFlags,
        NestedArtboardHostFlags::inactiveMarked);
    return marked && skippableWhileCollapsed() ? QuietState::quiet
                                               : QuietState::busy;
  }
  if (enums::is_flag_set(
          m_hostFlags,
          NestedArtboardHostFlags::pendingStatefulBinding))
  {
    return QuietState::busy;
  }
  // A quantized clock moves on every advance.
  if (quantize() >= 0)
  {
    return QuietState::never;
  }
  // The machines first, as a list checks its rows' (see
  // ArtboardComponentList::rowQuietState): they are cheap, and usually what
  // is busy.
  for (auto animation : m_NestedAnimations)
  {
    // Linear and remap animations play on time this can't see into.
    if (!animation->is<NestedStateMachine>())
    {
      return QuietState::never;
    }
    auto machine = animation->as<NestedStateMachine>()->stateMachineInstance();
    if (machine == nullptr)
    {
      continue;
    }
    auto state = machine->rowQuietState();
    if (state != QuietState::quiet)
    {
      return state;
    }
  }
  return m_Instance->rowQuietState();
}

bool NestedArtboard::skippableWhileCollapsed()
{
  for (auto animation : m_NestedAnimations)
  {
    if (!animation->is<NestedStateMachine>())
    {
      continue;
    }
    // Not every change such a machine reads reports a wake (a component
    // trigger fired directly, a bind already dirty, a binding still to be
    // made), so only the advance's note keeps it from taking one made while
    // it couldn't run.
    auto machine = animation->as<NestedStateMachine>()->stateMachineInstance();
    if (machine != nullptr && machine->stateMachine()->readsPendingChanges())
    {
      return false;
    }
  }
  return m_Instance == nullptr || m_Instance->hostedSkippableWhileCollapsed();
}

void NestedArtboard::hostedRowWoke(Artboard* artboard, uint32_t row)
{
  // Collapsed, the change is one the next advance has to note.
  m_hostFlags &= ~NestedArtboardHostFlags::inactiveMarked;
  // The row this host is on, if it is quiet, can change too.
  wakeRow();
}

void NestedArtboard::isPausedChanged() { wakeRow(); }

void NestedArtboard::quantizeChanged() { wakeRow(); }

void NestedArtboard::wakeRow()
{
  if (auto host = artboard())
  {
    host->wakeIfQuietRow();
  }
}

void NestedArtboard::reset() {}

void NestedArtboard::file(File* value) { m_file = value; }

File* NestedArtboard::file() const { return m_file; }

const File* NestedArtboard::foreignFile() const
{
  const File* source = m_mountedBindable != nullptr ? m_mountedBindable->file()
                                                    : nullptr;
  return m_file != nullptr && source != m_file ? source : nullptr;
}

int NestedArtboard::referencedArtboardId() { return artboardId(); }

void NestedArtboard::remount()
{
  if (m_Instance == nullptr)
  {
    // Deferred, there is nothing to replace, but what its nested inputs
    // hold was set for the item it hosted: a fresh instance's machines start
    // from the source's values, so these do too.
    if (isInstanceDeferred())
    {
      resetHeldInputs();
    }
    return;
  }
  // The outgoing instance leaves the trees it joined while it is whole, as
  // in swapArtboard.
  m_Instance->cleanupFocusTree();
  m_Instance->cleanupSemanticTree();
  // The nested animations' instances run on the outgoing artboard, so they
  // go first.
  for (auto* animation : m_NestedAnimations)
  {
    animation->releaseDependencies();
  }
  // The new machines take their nested inputs' values. A value set while
  // this host was deferred is still held there, so it goes back to the
  // source's before they start, as on the deferred path. With machines from
  // here on, nothing more can be held, so later remounts, and those of a
  // host that never deferred, skip it: a list remounts rows as it scrolls.
  if (enums::is_flag_set(m_hostFlags, NestedArtboardHostFlags::heldInputs))
  {
    resetHeldInputs();
    m_hostFlags &= ~NestedArtboardHostFlags::heldInputs;
  }
  auto source = m_Instance->artboardSource();
  m_cumulatedSeconds = 0.0f;
  // On the outgoing instance's factory, which may not be the file's (see
  // Artboard::reinstanceNestedArtboards). Destroys the outgoing instance,
  // with this host still its host.
  referencedArtboard(
      source->instance<ArtboardInstance>(m_Instance->factory()).release());
  for (auto* animation : m_NestedAnimations)
  {
    animation->initializeAnimation(m_Instance.get());
  }
  addDirt(ComponentDirt::Filthy);
}

// An instance's objects are its source's clones, in the same order, so an
// object's source sits at its index in the source artboard. Null when
// [object] isn't on an instance, or the source there isn't of its type.
static const Core* sourceObjectOf(const Artboard* host, const Core* object)
{
  const Artboard* source = host != nullptr ? host->artboardSource() : nullptr;
  if (source == nullptr || source == host)
  {
    return nullptr;
  }
  uint32_t id = host->idOf(const_cast<Core*>(object));
  const auto& objects = source->objects();
  if (id == 0 || id >= objects.size() || objects[id] == nullptr ||
      objects[id]->coreType() != object->coreType())
  {
    return nullptr;
  }
  return objects[id];
}

Artboard* NestedArtboard::deferredSource() const
{
  auto* sourceHost = sourceObjectOf(artboard(), this);
  if (sourceHost == nullptr ||
      sourceHost->as<NestedArtboard>()->artboardId() != artboardId())
  {
    return nullptr;
  }
  return sourceHost->as<NestedArtboard>()->m_referencedArtboard;
}

#ifdef TESTING
int NestedArtboard::testing_heldInputResets = 0;
std::atomic<uint64_t> NestedArtboard::sm_bindWalkSkips{0};
#endif

void NestedArtboard::resetHeldInputs()
{
#ifdef TESTING
  testing_heldInputResets++;
#endif
  for (auto* animation : m_NestedAnimations)
  {
    if (!animation->is<NestedStateMachine>())
    {
      continue;
    }
    auto* machine = animation->as<NestedStateMachine>();
    for (size_t i = 0; machine->input(i) != nullptr; i++)
    {
      auto* input = machine->input(i);
      auto* source = sourceObjectOf(artboard(), input);
      input->resetHeld(source != nullptr ? source->as<NestedInput>() : nullptr);
    }
  }
}

void NestedArtboard::mountDeferred()
{
  if (!isInstanceDeferred())
  {
    return;
  }
  m_hostFlags &= ~NestedArtboardHostFlags::deferredInstance;
  Artboard* source = deferredSource();
  Artboard* host = artboard();
  if (source == nullptr || host == nullptr)
  {
    // Left empty, as a clone of a host with nothing to reference is.
    m_deferredEventListeners = nullptr;
    return;
  }
  // On the hosting instance's factory, as Artboard::reinstanceNestedArtboards
  // leaves the hosts that were instanced with it.
  referencedArtboard(
      source->instance<ArtboardInstance>(host->factory()).release());
  // Artboard::volume reaches only the instances that exist when it is set,
  // so this one takes the hosting artboard's now.
  m_Instance->volume(host->volume());
  // Wires each machine to the host's focus manager, when it has one, and
  // applies the nested inputs' values (held while there was no machine).
  for (auto* animation : m_NestedAnimations)
  {
    animation->initializeAnimation(m_Instance.get());
  }
  // A machine's initializeAnimation built the focus tree under the host's
  // closest focus node; without one, build it here, as the load-time walk
  // (Artboard::buildFocusTree) does for every host.
  if (!hasNestedStateMachines())
  {
    syncNestedFocusTree(FocusData::findClosestFocusNode(this));
  }
  if (m_deferredEventListeners != nullptr)
  {
    auto listeners = m_deferredEventListeners->nestedEventListeners();
    m_deferredEventListeners = nullptr;
    for (auto* animation : m_NestedAnimations)
    {
      NestedEventNotifier* notifier = nullptr;
      if (animation->is<NestedStateMachine>())
      {
        notifier = animation->as<NestedStateMachine>()->stateMachineInstance();
      }
      else if (animation->is<NestedLinearAnimation>())
      {
        notifier = animation->as<NestedLinearAnimation>()->animationInstance();
      }
      if (notifier == nullptr)
      {
        continue;
      }
      notifier->setNestedArtboard(this);
      for (auto* listener : listeners)
      {
        notifier->addNestedEventListener(listener);
      }
    }
  }
  // The data context this host was handed while it had nothing to pass it
  // to. A stateful host binds on its next advance (referencedArtboard
  // scheduled it).
  if (m_viewModelInstance != nullptr)
  {
    bindViewModelInstance(m_viewModelInstance, m_dataContext);
  }
  else if (m_dataContext != nullptr)
  {
    internalDataContext(m_dataContext);
  }
  auto* semanticManager = host->semanticManager();
  if (semanticManager != nullptr)
  {
    auto parentNode = SemanticData::findClosestSemanticNode(this);
    m_Instance->buildSemanticTree(
        semanticManager,
        parentNode != nullptr ? parentNode : host->semanticBoundaryNode());
  }
  addDirt(ComponentDirt::Filthy);
}

void NestedArtboard::deferNestedEventListener(NestedEventListener* listener)
{
  if (m_deferredEventListeners == nullptr)
  {
    m_deferredEventListeners = std::make_unique<NestedEventNotifier>();
  }
  m_deferredEventListeners->addNestedEventListener(listener);
}

Artboard* NestedArtboard::sourceArtboard()
{
  return m_referencedArtboard != nullptr ? m_referencedArtboard->decoded()
                                         : nullptr;
}

void NestedArtboard::referencedArtboard(Artboard* artboard)
{
  assert(artboard != nullptr);
  ArtboardReferencer::referencedArtboard(artboard);
  nest(artboard);
  tryScheduleBindStateful();
}

AABB NestedArtboard::localBounds() const
{
  return m_Instance != nullptr ? m_Instance->bounds() : AABB();
}

BoundsFidelity NestedArtboard::paintedWorldBounds(AABB* out)
{
  if (hostedMesh() != nullptr)
  {
    // The mesh moves pixels anywhere; the box doesn't bound them.
    return BoundsFidelity::none;
  }
  ArtboardInstance* nested = artboardInstance();
  if (nested == nullptr)
  {
    return BoundsFidelity::none;
  }
  const BoundsFidelity fidelity = paintedBoundsFromLocal(
      nested->bounds(),
      worldTransform(),
      nullptr,
      out);
  if (fidelity == BoundsFidelity::none)
  {
    return fidelity;
  }
  // A nested artboard that clips cannot paint outside its own bounds. One
  // that does not can paint arbitrarily far past them, and finding out how
  // far means measuring its whole draw list -- so say "about here" and let
  // the caller pad.
  return nested->clip() ? fidelity : BoundsFidelity::approximate;
}

#ifdef WITH_RIVE_EDITOR
void NestedArtboard::editorPrepareForDestruction()
{
  clearNestedAnimations();
  if (m_boundNestedStateMachine)
    m_boundNestedStateMachine->releaseDependencies();
  unbind();
  Component::editorPrepareForDestruction();
}

#endif
