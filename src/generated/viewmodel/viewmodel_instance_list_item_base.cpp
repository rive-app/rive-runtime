#include "rive/generated/viewmodel/viewmodel_instance_list_item_base.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"

using namespace rive;

Core* ViewModelInstanceListItemBase::clone() const
{
    auto cloned = new ViewModelInstanceListItem();
    cloned->copy(*this);
    return cloned;
}
