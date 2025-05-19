#ifndef _RIVE_DATA_BIND_LIST_ITEM_PROVIDER_HPP_
#define _RIVE_DATA_BIND_LIST_ITEM_PROVIDER_HPP_
namespace rive
{
class ViewModelInstanceListItem;

class DataBindListItemConsumer
{
public:
    static DataBindListItemConsumer* from(Core* component);

    virtual void updateList(int propertyKey,
                            std::vector<ViewModelInstanceListItem*>* list) = 0;
};
} // namespace rive

#endif