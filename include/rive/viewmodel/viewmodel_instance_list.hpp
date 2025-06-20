#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_list_base.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceList : public ViewModelInstanceListBase
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
    Core* clone() const override;
    void advanced() override;

protected:
    std::vector<rcp<ViewModelInstanceListItem>> m_ListItems;
    void propertyValueChanged();
};
} // namespace rive

#endif