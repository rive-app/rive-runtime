#ifndef _RIVE_DATA_BIND_CONTAINER_HPP_
#define _RIVE_DATA_BIND_CONTAINER_HPP_
#include <cstddef>
#include <vector>

namespace rive
{
class DataContext;
class DataBind;
class DataBindContainer
{
public:
    virtual void updateDataBinds(bool applyTargetToSource = true);
    void addDataBind(DataBind* dataBind);
    void removeDataBind(DataBind* dataBind);
    const std::vector<DataBind*> dataBinds() const { return m_dataBinds; }
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
    std::vector<DataBind*> m_persistingDataBinds;
    // Push-driven toSource binds waiting to apply target → source. Kept
    // separate from m_dirtyDataBinds so updateDataBinds can run the
    // target→source pass *before* the source→target pass. Without this, a
    // target→source apply triggered later in the same updateDataBinds call
    // would see a source value updated by a sibling toTarget bind that ran
    // first.
    std::vector<DataBind*> m_dirtyToSourceDataBinds;
    std::vector<DataBind*> m_pendingDirtyToSourceDataBinds;
    std::vector<DataBind*> m_dirtyDataBinds;
    std::vector<DataBind*> m_pendingDirtyDataBinds;
    std::vector<DataBind*> m_pendingAdditions;
    std::vector<DataBind*> m_pendingRemovals;
    DataContext* m_dataContext = nullptr;
    bool m_isProcessing = false;
};
} // namespace rive

#endif