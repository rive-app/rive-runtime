#ifndef _RIVE_DATA_BIND_LIST_ITEM_PROVIDER_HPP_
#define _RIVE_DATA_BIND_LIST_ITEM_PROVIDER_HPP_
#include "rive/core.hpp"
#include "rive/refcnt.hpp"
namespace rive
{
class ViewModelInstanceListItem;

class DataBindListItemConsumer
{
public:
  static DataBindListItemConsumer* from(Core* component);

  virtual void updateList(
      std::vector<rcp<ViewModelInstanceListItem>>* list) = 0;

  // What the bound list comes from, told before each updateList: the list
  // property a plain bind reads, or null when a converter makes the list.
  // Only compared, never read through.
  virtual void boundListSource(const Core* /*source*/) {}
};
} // namespace rive

#endif