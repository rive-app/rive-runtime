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
#ifdef WITH_RIVE_EDITOR
  void editorPrepareForDestruction() override;
#endif
  ~ViewModelInstanceList();
  // These APIs accept an owning rcp. For an arena-owned raw item, pass
  // ref_rcp(item), not rcp(item): the arena keeps its initial reference.
  // Insertion copies that rcp, so the list needs no arena-specific ref().
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
  /// Takes out every item showing [viewModelInstance] in one pass, and
  /// appends them to [removed] when given.
  void removeAllItemsWithViewModelInstance(
      ViewModelInstance* viewModelInstance,
      std::vector<rcp<ViewModelInstanceListItem>>* removed = nullptr);
  void updateList(std::vector<rcp<ViewModelInstanceListItem>>* list) override;
  Core* clone() const override;
  void parentViewModelInstance(ViewModelInstance* parent)
  {
    m_parentViewModelInstance = parent;
  }
  ViewModelInstance* parentViewModelInstance()
  {
    return m_parentViewModelInstance;
  }
  // Drops the parent, which the items' instances stop naming too.
  void detachParent();
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
  // Index symbols are written in two halves, so every index is in place
  // before any listener runs: a listener that changes this list then finds
  // it numbered, and its change renumbers in turn. writeListIndices writes
  // the items from [start] on with their listeners held back and returns
  // the bound it wrote to; reportListIndices then reports the changes in
  // that range. Both re-read the live size, since a listener (or a tools
  // callback, which is not held back) may take items out, and the report
  // stops at the bound it was given, so a listener that keeps adding cannot
  // keep it going. Items whose view model declares no index are skipped.
  size_t writeListIndices(size_t start);
  void reportListIndices(size_t start, size_t end);
  void assignListIndices(size_t start);
  // An instance shown by several items has one index symbol, which the
  // host's pass leaves at the last item showing it. After a removal the
  // surviving occurrence before the change point may be that one, so this
  // writes the instance's last position, held back like the rest, or
  // nothing when it is gone or has no index.
  void writeLastIndexOf(const rcp<ViewModelInstance>& instance);
};
} // namespace rive

#endif