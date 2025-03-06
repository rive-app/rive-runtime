#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_list_item_base.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceListItem : public ViewModelInstanceListItemBase
{
private:
    rcp<ViewModelInstance> m_viewModelInstance;
    Artboard* m_artboard;

public:
    void viewModelInstance(rcp<ViewModelInstance> value)
    {
        m_viewModelInstance = value;
    };
    rcp<ViewModelInstance> viewModelInstance() { return m_viewModelInstance; }
    void artboard(Artboard* value) { m_artboard = value; };
    Artboard* artboard() { return m_artboard; }
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif