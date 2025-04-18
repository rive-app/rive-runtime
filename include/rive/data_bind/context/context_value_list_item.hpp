#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_LIST_ITEM_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_LIST_ITEM_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
namespace rive
{
class DataBindContextValueListItem
{

private:
    ViewModelInstanceListItem* m_ListItem = nullptr;

public:
    DataBindContextValueListItem(ViewModelInstanceListItem* listItem);
    ViewModelInstanceListItem* listItem() { return m_ListItem; };
};
} // namespace rive

#endif