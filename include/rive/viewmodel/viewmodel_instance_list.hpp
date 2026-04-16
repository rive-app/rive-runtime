#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_HPP_
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_list_base.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceList;
typedef void (*ViewModelListChanged)(ViewModelInstanceList* vmi);
#endif
class ViewModelInstanceList : public ViewModelInstanceListBase,
                              public DataBindListItemConsumer
{
public:
    ~ViewModelInstanceList();
    void addItem(rcp<ViewModelInstanceListItem> listItem);
    bool addItemAt(rcp<ViewModelInstanceListItem> listItem, int index);
    void internalAddItem(rcp<ViewModelInstanceListItem> listItem);
    void removeItem(int index);
    void removeItem(rcp<ViewModelInstanceListItem> listItem);
    Span<rcp<ViewModelInstanceListItem>> listItems() { return m_ListItems; }
    rcp<ViewModelInstanceListItem> item(uint32_t index);
    void swap(uint32_t index1, uint32_t index2);
    rcp<ViewModelInstanceListItem> pop();
    rcp<ViewModelInstanceListItem> shift();
    void removeAllItems();
    void removeAllItemsWithViewModelInstance(
        ViewModelInstance* viewModelInstance);
    void updateList(std::vector<rcp<ViewModelInstanceListItem>>* list) override;
    Core* clone() const override;
    void advanced() override;
    void parentViewModelInstance(ViewModelInstance* parent)
    {
        m_parentViewModelInstance = parent;
    }
    ViewModelInstance* parentViewModelInstance()
    {
        return m_parentViewModelInstance;
    }
#ifdef WITH_RIVE_TOOLS
    void onChanged(ViewModelListChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelListChanged m_changedCallback = nullptr;
#endif

protected:
    std::vector<rcp<ViewModelInstanceListItem>> m_ListItems;
    void propertyValueChanged();

private:
    ViewModelInstance* m_parentViewModelInstance = nullptr;
};
} // namespace rive

#endif