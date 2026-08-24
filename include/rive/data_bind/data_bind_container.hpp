#ifndef _RIVE_DATA_BIND_CONTAINER_HPP_
#define _RIVE_DATA_BIND_CONTAINER_HPP_
#include <cstddef>
#include <vector>
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
    virtual void updateDataBinds(bool applyTargetToSource = true);
    void addDataBind(DataBind* dataBind);
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
    void deleteDataBinds();
    bool advanceDataBinds(float);
    void bindDataBindsFromContext(DataContext*);
    void unbindDataBinds();
    void sortDataBinds();

private:
    void updateDataBind(DataBind* dataBind, bool applyTargetToSource);
    std::vector<DataBind*> m_dataBinds;
    // The hot queue — see DataBindQueues above for why this one is inline and
    // the other six are not.
    std::vector<DataBind*> m_dirtyDataBinds;
    Sidecar<DataBindQueues> m_queues;
    DataContext* m_dataContext = nullptr;
    bool m_isProcessing = false;
};
} // namespace rive

#endif
