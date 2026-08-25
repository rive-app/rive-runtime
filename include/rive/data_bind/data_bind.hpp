#ifndef _RIVE_DATA_BIND_HPP_
#define _RIVE_DATA_BIND_HPP_
#include "rive/component_dirt.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/viewmodel/viewmodel_value_dependent.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>
namespace rive
{
class DataBindContextValue;
class DataBindContainer;
class File;
#ifdef WITH_RIVE_TOOLS
class DataBind;
typedef void (*DataBindChanged)();
#endif
class DataBind : public DataBindBase, public ViewModelValueDependent
{
public:
    ~DataBind();
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
    virtual void updateSourceBinding(bool invalidate = false);
    virtual void update(ComponentDirt value);
    void updateDependents();
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. Bodies in editor_native/native/src/
    // editor/data_bind/data_bind_editor.cpp — the setter also mirrors
    // the runtime body's observer unsubscribe/resubscribe for the
    // raw-pointer (pump-runtime) fallback.
    Core* target() const;
    void target(Core* value);
#else
    Core* target() const { return m_target; };
    // Body in data_bind.cpp — swaps the property-observer subscription
    // when the target changes.
    void target(Core* value);
#endif
    virtual void bind();
    virtual void unbind();
    ComponentDirt dirt() { return m_Dirt; };
    void dirt(ComponentDirt value) { m_Dirt = value; };
    void addDirt(ComponentDirt value, bool recurse) override;
    DataConverter* converter() const { return m_dataConverter; };
    void converter(DataConverter* value) { m_dataConverter = value; };
    ViewModelInstanceValue* source() const { return m_Source.get(); };
    void source(rcp<ViewModelInstanceValue> value);
    void clearSource();
    bool toSource();
    bool toTarget();
    bool targetSupportsPush() const;
    bool isNameBased();
    bool canSkip();
    bool isMainToSource();
    bool sourceToTargetRunsFirst();
    bool advance(float elapsedTime);
    void suppressDirt(bool value) { setFlag(Flag::SuppressDirt, value); };
    void file(File* value);
    File* file() const;
    DataType outputType();
    DataType sourceOutputType();
    // m_container is a back-pointer set when this DataBind joins a
    // container. Containers manage their DataBinds' lifetime (see
    // `DataBindContainer::deleteDataBinds` /
    // `clearEditorDataBinds`), so the DataBind doesn't outlive its
    // container — same shared-lifetime pattern as
    // `LayoutComponent::m_style`. No handle migration here; one of
    // the three DataBindContainer implementers
    // (`StateMachineInstance`) isn't a Core anyway, so the handle
    // approach can't even cover it.
    inline DataBindContainer* container() const { return m_container; }
    void container(DataBindContainer*);
    // Public on master (data_bind_container_test pokes it directly);
    // keep the accessors above for editor-side call sites.
    DataBindContainer* m_container = nullptr;
    void collapse(bool collapsed);
    void initialize();
    void relinkDataBind() override;
    bool inDirtyList() const { return hasFlag(Flag::InDirtyList); }
    void inDirtyList(bool value) { setFlag(Flag::InDirtyList, value); }
    bool inPersistingList() const { return hasFlag(Flag::InPersistingList); }
    void inPersistingList(bool value)
    {
        setFlag(Flag::InPersistingList, value);
    }
    // Persisted direction of the change currently in flight. Set by addDirt
    // from the Bindings / BindingsTarget bit it receives, and read by a
    // converter's markConverterDirty so a multi-frame interpolation keeps
    // re-asserting the SAME direction (per-frame dirt is cleared every update
    // and can't carry the origin across frames). Defaults to source (false),
    // matching the historical hardcoded markConverterDirty behavior.
    bool targetOrigin() const { return hasFlag(Flag::TargetOrigin); }

    // Intrusive observer-list linkage. Used by Core to chain DataBinds that
    // have subscribed to push notifications for a given (target, propertyKey).
    DataBind* nextObserver() const { return m_nextObserver; }
    void setNextObserver(DataBind* next) { m_nextObserver = next; }
    DataBind*& nextObserverRef() { return m_nextObserver; }
    // Called by ~Core() when the target is destroyed out from under us.
    // Clears m_target, the list linkage, AND the Observing flag so
    // subsequent unbind()/target() calls don't try to unsubscribe from the
    // dead Core (which would deref freed memory in removePropertyObserver).
    void onTargetDestroyed()
    {
        m_nextObserver = nullptr;
        m_target = nullptr;
        setFlag(Flag::Observing, false);
    }

#ifdef WITH_RIVE_EDITOR
    // Editor-only ownership marker. Set by
    // `DataBindContainer::addDataBindForEditor` for coop-hydrated
    // DataBinds whose lifetime is owned by `EditorFile::m_arena`,
    // not the container. `deleteDataBinds()` checks this flag and
    // skips the `delete` so we don't double-free arena entries
    // alongside the importer-owned entries that share the same
    // `m_dataBinds` list. Mirrors Dart's "DataBinds are file-owned
    // children of Artboard" model — which works there because GC
    // handles cleanup. The flag is the C++ way to express the same
    // "owned elsewhere" semantic.
    bool isEditorOwned() const { return m_isEditorOwned; }
    void markEditorOwned() { m_isEditorOwned = true; }
#endif

private:
    // State bits packed into one byte. Each bool used to live in its own
    // 1B slot with up to 7B of padding between fields, costing ~16B per
    // DataBind across the flags. Packed they fit in one byte.
    enum class Flag : uint8_t
    {
        Collapsed = 1 << 0,
        InDirtyList = 1 << 1,
        InPersistingList = 1 << 2,
        SuppressDirt = 1 << 3,
        Observing = 1 << 4,
        // Latched direction of the in-flight change: set means the change
        // originated from the target (target→source), clear means the source
        // (source→target). Persists across frames so interpolators re-assert
        // it.
        TargetOrigin = 1 << 5,
    };
    uint8_t m_flags = 0;
    bool hasFlag(Flag f) const
    {
        return (m_flags & static_cast<uint8_t>(f)) != 0;
    }
    void setFlag(Flag f, bool value)
    {
        if (value)
        {
            m_flags |= static_cast<uint8_t>(f);
        }
        else
        {
            m_flags &= static_cast<uint8_t>(~static_cast<uint8_t>(f));
        }
    }
    DataBind* m_nextObserver = nullptr;
#ifdef WITH_RIVE_EDITOR
    bool m_isEditorOwned = false;
#endif

protected:
    ComponentDirt m_Dirt = ComponentDirt::None;
    Core* m_target = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage for m_target only. Editor-flow
    // Cores resolve through the handle (so a deleted target resolves
    // to nullptr); pump-runtime Cores read the raw pointer.
    CoreHandle m_targetHandle;
#endif
    rcp<ViewModelInstanceValue> m_Source = nullptr;
    DataBindContextValue* m_ContextValue = nullptr;
    DataConverter* m_dataConverter = nullptr;
    bool bindsOnce();
    // Dirt to (re)sync a bind in both supported directions on bind/reconcile —
    // Bindings for toTarget, BindingsTarget for toSource, both for TwoWay.
    ComponentDirt reconcileDirt();
    File* m_file;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(DataBindChanged callback) { m_changedCallback = callback; }
    DataBindChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif