#ifndef _RIVE_COMPONENT_HPP_
#define _RIVE_COMPONENT_HPP_
#include "rive/component_dirt.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/dependency_helper.hpp"
#include "rive/lazy_vector.hpp"
#include "rive/math/vec2d.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif

#include <vector>
#include <functional>

namespace rive
{
class ContainerComponent;
class Artboard;
class DataBind;

class Component : public ComponentBase,
                  public DependencyHelper<Artboard, Component, Component>
{
    friend class Artboard;

private:
    // Raw parent pointer — sole storage in runtime build, fallback
    // in editor build for Cores not allocated via `arena.allocate`
    // (e.g., pump-runtime instances loaded via `File::import`,
    // whose `editorArena()` is null).
    ContainerComponent* m_Parent = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase B: generational handle path. Used for editor-
    // flow Cores (top-level + asset + per-instance clones — all
    // arena-allocated, all with `editorArena()` set). Stale handle
    // (parent slot freed + generation bumped) → resolves to nullptr;
    // iteration is naturally null-safe.
    //
    // `parent()` and `setParentForEditor()` dispatch on
    // `editorArena()`: non-null → use this handle; null → fall back
    // to `m_Parent` raw. Slice 7-revised killed the runtime
    // importer in editor flow, so every editor-flow Core IS arena-
    // allocated; the raw fallback only catches pump-runtime
    // instances which have atomic lifetime (no mid-frame deletion)
    // and don't need handle protection.
    CoreHandle m_ParentHandle;

public:
    // Editor-installable hook fired from `parentIdChanged()` after
    // the generated setter swaps `m_ParentId`. editor_native
    // registers a function that does the actual re-parenting work
    // (typed-list swap, m_Artboard re-resolve, cycle prevention).
    // Runtime never installs anything → call no-ops.
    using OnParentIdChangedCallback = void (*)(Component*);

private:
    static OnParentIdChangedCallback s_onParentIdChanged;
#endif

    unsigned int m_GraphOrder;
    Artboard* m_Artboard = nullptr;
    LazyVector<DataBind*> m_collapsables;

protected:
    ComponentDirt m_Dirt = ComponentDirt::Filthy;
    void updateCollapsables();

public:
    // Required by DependencyHelper's CRTP base — the artboard IS the
    // dependency root for component graphs.
    Artboard* dependencyRoot() const { return m_Artboard; }

    virtual bool collapse(bool value);
    inline Artboard* artboard() const { return m_Artboard; }
    bool validate(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
#ifdef WITH_RIVE_EDITOR
    // Body in `editor_native/native/src/editor/component_editor.cpp`
    // — needs the full `ContainerComponent` definition for the
    // `as<>` upcast, which would create a circular include here
    // (ContainerComponent extends Component).
    ContainerComponent* parent() const;
#else
    inline ContainerComponent* parent() const { return m_Parent; }
#endif

#ifdef WITH_RIVE_EDITOR
    // Edit-time parent-transition hook. Mirrors Dart's parent-setter
    // chain (parentIdChanged → parent.internalAddX): subclasses with
    // a typed child list on the parent (e.g. Path::m_Vertices,
    // Mesh::m_Vertices, TransformComponent::m_Constraints) override
    // this to register/unregister themselves on parent transitions.
    //
    // Either pointer can be null:
    //   from == nullptr, to != nullptr → first registration (initial
    //                                    coop hydration or recovery
    //                                    from orphan).
    //   from != nullptr, to == nullptr → unregister (parent deleted
    //                                    or child being deleted).
    //   from != nullptr, to != nullptr → re-parent (user-driven edit
    //                                    or undo/redo).
    //
    // Called from three places:
    //   1. Dispatcher Pass 4.5 after `onAddedClean` for newly-
    //      hydrated, non-orphan Components (initial registration).
    //   2. `Component::parentIdChanged()` editor override, when the
    //      generated `parentId(value)` setter fires post-validation
    //      (edit-time re-parent).
    //   3. `EditorFile::removeObject` before the arena frees a Core
    //      (clean unregister).
    //
    // Subclasses should be idempotent on `to` (no double-add) and
    // tolerant of `from == nullptr` (no removal). The runtime
    // import path doesn't compile or link this hook.
    virtual void editorParentChanged(ContainerComponent* /*from*/,
                                     ContainerComponent* /*to*/)
    {}

    // Edit-time re-parent dispatch (Slice 6 Phase G). The generated
    // setter `parentId(value)` calls `parentIdChanged()` after the
    // new value lands; we override it here to fan out into a Core-
    // installed callback. editor_native installs the callback at
    // library load (see proprietary/core_hook.cpp) — the actual
    // re-parenting work (resolve new parent, fire
    // editorParentChanged, swap m_Parent + m_Artboard, update
    // generic + typed child lists, cycle prevention) lives there.
    //
    // Runtime never installs a callback, so this override no-ops at
    // runtime — same observable behavior as the empty default.
    void parentIdChanged() override;

    static void installParentIdChangedCallback(
        OnParentIdChangedCallback callback)
    {
        s_onParentIdChanged = callback;
    }

    // Mirror of Dart `Component.validate()` (rive_core/component.dart:409):
    //   `(parent != null && artboard != null) || canBeOrphaned`
    // Used by the dispatcher's cull pass (post-onAddedClean) to
    // remove Cores whose ancestor chain is broken — typically because
    // an unknown typeKey (forward-compat features the editor build
    // doesn't recognize) returned null from `createObject`, so
    // descendants of that core have unresolvable parentIds and stay
    // permanently orphaned. Culling them before `buildDependencies`
    // / `updatePass` prevents `update()` derefs on `parent()` /
    // `artboard()` / `renderPaint()` from crashing.
    //
    // Default: parent and artboard both non-null. Subclasses can
    // override to opt into orphan-tolerance (e.g., the Artboard
    // itself returns true unconditionally — it has no parent).
    virtual bool editorIsResolved() const
    {
        return parent() != nullptr && artboard() != nullptr;
    }

    // Editor-only: clear the back-pointer to parent. Called by
    // `EditorFile::removeObject` on every direct child of a Core
    // being freed, BEFORE the free runs, so subsequent cull
    // iterations don't deref a freed parent pointer when they
    // process the now-orphan child. Body in `component_editor.cpp`.
    void editorClearParent();
#endif
    void addCollapsable(DataBind* collapsable);
#ifdef WITH_RIVE_EDITOR
    /// Editor-only: scrub `component` from this Component's
    /// dependents list. Used by `editorOrphanCore` BEFORE freeing
    /// the dying Component's arena slot — without this, every
    /// live Component that recorded the dying one as a dependent
    /// keeps a dangling pointer, and the next
    /// `Artboard::sortDependencies` walk segfaults on
    /// `dying->dependents()`.
    void editorRemoveDependent(Component* component)
    {
        removeDependent(component);
    }
    /// Editor-only: drop every entry from this component's dependents
    /// list. Used by `EditorFile::finalizeBatch` to wipe stale
    /// cross-batch references before re-running `buildDependencies`
    /// — coop hydration can add a Component as a dependent in batch
    /// 1 and then orphan its parent in batch 2; without a fresh-
    /// start clear, `sortDependencies` walks the orphan back into
    /// `m_DependencyOrder` and `updatePass` crashes on its now-null
    /// `parent()` / `artboard()`.
    void editorClearDependents() { clearDependentsForEditor(); }
#endif

    // TODO: re-evaluate when more of the lib is complete...
    // These could be pure virtual but we define them empty here to avoid
    // having to implement them in a bunch of concrete classes that
    // currently don't use this logic.
    virtual void buildDependencies() {}
    virtual void onDirty(ComponentDirt dirt) {}
    virtual void update(ComponentDirt value) {}

    unsigned int graphOrder() const { return m_GraphOrder; }
    bool addDirt(ComponentDirt value, bool recurse = false);
    inline bool hasDirt(ComponentDirt flag) const
    {
        return (m_Dirt & flag) == flag;
    }
    static inline bool hasDirt(ComponentDirt value, ComponentDirt flag)
    {
        return (value & flag) != ComponentDirt::None;
    }

    StatusCode import(ImportStack& importStack) override;

    virtual bool isCollapsed() const
    {
        return (m_Dirt & ComponentDirt::Collapsed) == ComponentDirt::Collapsed;
    }
    virtual bool hitTestPoint(const Vec2D& position,
                              bool skipOnUnclipped,
                              bool isPrimaryHit);
#ifdef TESTING
    ComponentDirt dirt() { return m_Dirt; }
#endif
#ifdef WITH_RIVE_EDITOR
    /// Editor-only: re-wire `m_Artboard` + `m_Parent` post-hydration.
    /// Pass 3 `onAddedDirty` may run before parents are in the arena
    /// for deeply-nested components, leaving `m_Artboard` null and
    /// the CRTP `dependencyRoot()` (which reads `m_Artboard`) null —
    /// silently breaking dirt propagation into the artboard's
    /// component pass.
    /// `EditorFile::finalizeBatch` calls this after every Pass 1-5
    /// completes, when the parent chain is fully hydrated.
    void setArtboardForEditor(Artboard* ab) { m_Artboard = ab; }
    // Body in `editor_native/native/src/editor/component_editor.cpp`.
    // Resolves through `editorArena()` to convert pointer ↔ handle.
    void setParentForEditor(ContainerComponent* p);
    /// Editor-only: clear `m_Dirt` to `None`. Components default to
    /// `Filthy` at construction; the runtime `.riv` path runs an
    /// initial `updateComponents` pass via `Artboard::initialize()`
    /// that consumes the Filthy state on first frame. editor_native
    /// has no equivalent — coop-loaded Cores stay Filthy after
    /// finalizeBatch, and any that later acquire the Collapsed bit
    /// (Solo's propagateCollapse runs in Pass E) get
    /// `Filthy | Collapsed = 0xFFFF`. updateComponents then skips
    /// them forever, so m_Dirt never clears and subsequent
    /// `addDirt` calls early-out — the dirty cascade silently
    /// breaks for collapsed subtrees and any sibling that shares
    /// dependents with them.
    void clearDirtForEditor() { m_Dirt = ComponentDirt::None; }

    /// Editor-only `ComponentFlags` bit positions. Mirrors dart's
    /// [packages/rive_core/lib/component_flags.dart](packages/rive_core/lib/component_flags.dart)
    /// 1:1 so the `flags()` byte read by the hierarchy panel decodes
    /// the same on both sides. The bits are unused at runtime — the
    /// guards that read them (`Drawable::isHidden`, hit-test locked
    /// skip, isolation filter) all live under `WITH_RIVE_EDITOR`.
    enum EditorFlag : uint32_t
    {
        EditorFlagHidden = 1u << 0,
        EditorFlagLocked = 1u << 1,
        EditorFlagIsolated = 1u << 5,
        // Inverse overrides (dart's ComponentFlags.visibleOverride /
        // unlockedOverride): break a hidden/locked state inherited
        // from an ancestor for this subtree. Mutually exclusive with
        // the paired flag on the same component — the hierarchy
        // panel's cycle keeps them coherent.
        EditorFlagVisibleOverride = 1u << 6,
        EditorFlagUnlockedOverride = 1u << 7,
    };

    /// True iff [flag] is set on this Component or any ancestor in
    /// the parent chain. Matches dart's
    /// `Component.isFlaggedRecursive` — the hidden/locked/isolated
    /// bits don't physically cascade to children (toggling a parent
    /// doesn't mass-set the kids), so the cascade gets evaluated on
    /// every check. Cheap walk — the chain is small.
    ///
    /// Body in `editor_native/native/src/editor/component_editor.cpp`
    /// — needs the full `ContainerComponent` definition for the
    /// upcast inside the loop, which would create a circular include
    /// here (ContainerComponent extends Component).
    bool isFlaggedRecursive(EditorFlag flag) const;

    /// Walk the PARENT chain (not this) for [flag] paired with its
    /// inverse [overrideFlag]: nearest override resolves false,
    /// nearest flag resolves true. Mirrors dart's
    /// `Component.parentResolvesFlag`. Body in component_editor.cpp.
    bool editorParentResolvesFlag(uint32_t flag, uint32_t overrideFlag) const;
#endif
};
} // namespace rive

#endif
