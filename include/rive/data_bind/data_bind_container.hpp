#ifndef _RIVE_DATA_BIND_CONTAINER_HPP_
#define _RIVE_DATA_BIND_CONTAINER_HPP_
#include <cstddef>
#include <vector>
#include "rive/refcnt.hpp"
#include "rive/sidecar.hpp"

namespace rive
{
class DataContext;
class DataBind;

// The data-bind work queues that most containers never touch, hoisted behind a
// Sidecar so DataBindContainer (a base of Artboard, StateMachineInstance, AND
// every DataConverter) stays small: 144 B inline becomes an 8 B pointer.

struct DataBindQueues
{
    std::vector<DataBind*> persisting;
    // Push-driven toSource binds waiting to apply target → source. Kept
    // separate from the dirty list so updateDataBinds can run the target→source
    // pass *before* the source→target pass. Without this, a target→source apply
    // triggered later in the same updateDataBinds call would see a source value
    // updated by a sibling toTarget bind that ran first.
    std::vector<DataBind*> dirtyToSource;
    std::vector<DataBind*> pendingDirtyToSource;
    std::vector<DataBind*> pendingDirty;
    std::vector<DataBind*> pendingAdditions;
    std::vector<DataBind*> pendingRemovals;
};

class DataBindContainer
{
public:
    // Declared here and defined in the .cpp so that rcp<DataContext>'s
    // ref/unref are only instantiated where DataContext is complete.
    // data_context.hpp reaches back into this header through
    // viewmodel_instance.hpp, so it cannot be included here, which leaves
    // DataContext forward-declared for every TU that sees this class. Any
    // special member left implicit — or a default member initializer on
    // m_dataContext — would odr-use ~rcp right here and fail to compile in
    // the TUs that never pull data_context.hpp in themselves.
    DataBindContainer();
    ~DataBindContainer();
    // Copying would alias the raw DataBind* lists and leave every bind's
    // container() back-pointer aimed at the original.
    DataBindContainer(const DataBindContainer&) = delete;
    DataBindContainer& operator=(const DataBindContainer&) = delete;
    virtual void updateDataBinds(bool applyTargetToSource = true);
    void addDataBind(DataBind* dataBind);
#ifdef WITH_RIVE_EDITOR
    // Editor-only DataBind add path. Coop-hydrated DataBinds are
    // owned by `EditorFile::m_arena`, not by this container — but
    // they still need to participate in `updateDataBinds`,
    // `bindDataBindsFromContext`, etc. This wraps `addDataBind` so
    // the entry gets all the working-set plumbing
    // (m_persistingDataBinds / m_dirtyDataBinds, container
    // back-pointer) but gets flagged so `deleteDataBinds()` skips
    // it at destruction. See `DataBind::isEditorOwned`.
    void addDataBindForEditor(DataBind* dataBind);
    // Remove every flagged (editor-owned) entry from this
    // container's lists. Importer-added (unflagged) entries are
    // preserved. Called by `EditorFile::finalizeBatch` at the start
    // of every batch so the subsequent DB pass can re-add freshly
    // and stay idempotent across batches.
    void clearEditorDataBinds();
#endif
    void removeDataBind(DataBind* dataBind);
    // Applies a single (source→target) data bind immediately if it is dirty.
    // Used to refresh per-instance keyframe value holders at read time so their
    // value is current regardless of where the batched updateDataBinds() falls
    // in the frame. A no-op when the bind is not dirty.
    void flushDataBind(DataBind* dataBind) { updateDataBind(dataBind, false); }
    const std::vector<DataBind*>& dataBinds() const { return m_dataBinds; }
    virtual void addDirtyDataBind(DataBind* dataBind);
    virtual void rebind() {};
    virtual void relinkDataContext() {};
    virtual void rebuildDataBind(DataBind*) {};

protected:
    // The one data context this container is bound to. Owning: Artboard and
    // StateMachineInstance create the context with make_rcp and hand it here,
    // and nothing else keeps it alive. Held by the base rather than by each
    // container so the pointer addDataBind() reads can never drift out of sync
    // with the one the container thinks it is bound to.
    const rcp<DataContext>& dataBindContext() const { return m_dataContext; }
    // Sets the context WITHOUT re-pointing the existing binds. Artboard needs
    // the member visible while it recurses into its artboard hosts, before the
    // binds are walked; it follows this with bindDataBindsFromContext().
    void dataBindContext(rcp<DataContext> dataContext);
    void deleteDataBinds();
    bool advanceDataBinds(float);
    // Sets the context and re-points every DataBindContext at it.
    void bindDataBindsFromContext(rcp<DataContext> dataContext);
    // Re-points every DataBindContext at the already-set context.
    void bindDataBindsFromContext();
    void unbindDataBinds();
    void sortDataBinds();

private:
    void updateDataBind(DataBind* dataBind, bool applyTargetToSource);
    std::vector<DataBind*> m_dataBinds;
    // The hot queue — see DataBindQueues above for why this one is inline and
    // the other six are not.
    std::vector<DataBind*> m_dirtyDataBinds;
    Sidecar<DataBindQueues> m_queues;
    // No default member initializer here on purpose; see the note above.
    rcp<DataContext> m_dataContext;
    bool m_isProcessing = false;
};
} // namespace rive

#endif
