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
    void addItem(ViewModelInstanceListItem* listItem);
    void insertItem(int index, ViewModelInstanceListItem* listItem);
    void removeItem(int index);
    void removeItem(ViewModelInstanceListItem* listItem);
    std::vector<ViewModelInstanceListItem*> listItems() { return m_ListItems; };
    ViewModelInstanceListItem* item(uint32_t index);
    void swap(uint32_t index1, uint32_t index2);
    Core* clone() const override;

protected:
    std::vector<ViewModelInstanceListItem*> m_ListItems;
    void propertyValueChanged();
};
} // namespace rive

#endif