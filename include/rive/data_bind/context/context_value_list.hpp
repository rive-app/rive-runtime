#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_LIST_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_LIST_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/context/context_value_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
namespace rive
{
class DataBindContextValueList : public DataBindContextValue
{

public:
    DataBindContextValueList(DataBind* m_dataBind);
    ~DataBindContextValueList();
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    void update(Core* target) override;
    virtual void applyToSource(Core* component,
                               uint32_t propertyKey,
                               bool isMainDirection) override;

private:
    std::vector<std::unique_ptr<DataBindContextValueListItem>> m_ListItemsCache;
    void insertItem(Core* target,
                    ViewModelInstanceListItem* viewModelInstanceListItem,
                    int index);
    void swapItems(Core* target, int index1, int index2);
    void popItem(Core* target);
};
} // namespace rive

#endif