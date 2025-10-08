#ifndef _RIVE_DATA_BIND_CONTAINER_HPP_
#define _RIVE_DATA_BIND_CONTAINER_HPP_
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
    const std::vector<DataBind*> dataBinds() const { return m_dataBinds; }
    virtual void addDirtyDataBind(DataBind* dataBind);

protected:
    void deleteDataBinds();
    bool advanceDataBinds(float);
    void bindDataBindsFromContext(DataContext*);
    void unbindDataBinds();
    void sortDataBinds();

private:
    bool updateDataBind(DataBind* dataBind, bool applyTargetToSource);
    std::vector<DataBind*> m_dataBinds;
    std::vector<DataBind*> m_persistingDataBinds;
    std::vector<DataBind*> m_dirtyDataBinds;
    std::vector<DataBind*> m_unprocessedDirtyDataBinds;
};
} // namespace rive

#endif