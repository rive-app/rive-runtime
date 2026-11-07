#ifndef _RIVE_DEPENDENT_LIST_HPP_
#define _RIVE_DEPENDENT_LIST_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace rive
{
// A set of dependents, each held once and kept in the order it was added,
// behind one pointer that stays null until the first one.
//
// One object can have a great many: a view model value has a dependent per
// bind to it, and a state machine condition is a bind of its own, so a value
// every row of a long list reads has one per condition per row. Scanning the
// list to keep each dependent once made adding N of them cost O(N^2), and so
// did removing them. Past kIndexedFrom dependents an index of their slots
// (open addressing, keyed by pointer) finds one without a scan. A removal
// then leaves a hole, which iteration skips and the list closes in place
// once holes are half of it. A shorter list scans and erases as before.
template <typename T> class DependentList
{
public:
  static constexpr size_t kIndexedFrom = 32;

  DependentList() = default;
  ~DependentList() { delete m_block; }
  DependentList(const DependentList& other)
  {
    for (auto dependent : other.view())
    {
      add(dependent);
    }
  }
  DependentList& operator=(const DependentList& other)
  {
    if (this != &other)
    {
      DependentList copy(other);
      std::swap(m_block, copy.m_block);
    }
    return *this;
  }
  DependentList(DependentList&& other) noexcept : m_block(other.m_block)
  {
    other.m_block = nullptr;
  }
  DependentList& operator=(DependentList&& other) noexcept
  {
    std::swap(m_block, other.m_block);
    return *this;
  }

  bool empty() const { return size() == 0; }
  size_t size() const
  {
    if (m_block == nullptr)
    {
      return 0;
    }
    auto index = m_block->index.get();
    return m_block->list.size() - (index != nullptr ? index->holes : 0);
  }

  // Adds dependent unless it's already here.
  void add(T* dependent)
  {
    // A hole is a null slot, which a null would match.
    if (dependent == nullptr)
    {
      return;
    }
    if (m_block == nullptr)
    {
      m_block = new Block();
    }
    auto& list = m_block->list;
    auto index = m_block->index.get();
    if (index == nullptr)
    {
      if (std::find(list.begin(), list.end(), dependent) != list.end())
      {
        return;
      }
      list.push_back(dependent);
      if (list.size() >= kIndexedFrom)
      {
        m_block->index.reset(new Index());
        rebuildIndex();
      }
      return;
    }
    size_t free;
    if (findSlot(dependent, &free) != kNone)
    {
      return;
    }
    if (index->table[free] == kEmpty)
    {
      index->used++;
    }
    index->table[free] = static_cast<uint32_t>(list.size() + 1);
    list.push_back(dependent);
    if (index->used * 2 > index->table.size())
    {
      // Holes go first, as their cells are part of what filled the table.
      if (index->holes > 0)
      {
        compact();
      }
      else
      {
        rebuildIndex();
      }
    }
  }

  void remove(T* dependent)
  {
    if (m_block == nullptr || dependent == nullptr)
    {
      return;
    }
    auto& list = m_block->list;
    auto index = m_block->index.get();
    if (index == nullptr)
    {
      list.erase(std::remove(list.begin(), list.end(), dependent), list.end());
      return;
    }
    size_t slot = findSlot(dependent, nullptr);
    if (slot == kNone)
    {
      return;
    }
    // The slot's cell stays: with the slot empty it matches nothing, and
    // findSlot probes past it.
    list[slot] = nullptr;
    index->holes++;
    if (index->holes * 2 > list.size())
    {
      compact();
    }
  }

  void reserve(size_t count)
  {
    if (count == 0)
    {
      return;
    }
    if (m_block == nullptr)
    {
      m_block = new Block();
    }
    m_block->list.reserve(count);
  }

  void clear()
  {
    delete m_block;
    m_block = nullptr;
  }

  // Every dependent, in the order added, with no holes.
  const std::vector<T*>& view() const
  {
    if (m_block == nullptr)
    {
      static const std::vector<T*> none;
      return none;
    }
    auto index = m_block->index.get();
    if (index != nullptr && index->holes > 0)
    {
      const_cast<DependentList*>(this)->compact();
    }
    return m_block->list;
  }

  // Calls visit with every dependent, in the order added. One added along
  // the way is visited too. Removing one along the way can shift the rest,
  // as erasing from a vector does, so visit shouldn't.
  template <typename F> void forEach(F&& visit) const
  {
    if (m_block == nullptr)
    {
      return;
    }
    // Holes go first. Closing them moves dependents to lower slots, so one
    // closed under the walk (view(), or an add that fills the index) would
    // move dependents not yet visited behind it. With none, only a removal
    // along the way leaves one to close.
    auto index = m_block->index.get();
    if (index != nullptr && index->holes > 0)
    {
      const_cast<DependentList*>(this)->compact();
    }
    // Indexed: the vector may grow under the walk.
    for (size_t i = 0; i < m_block->list.size(); i++)
    {
      if (auto dependent = m_block->list[i])
      {
        visit(dependent);
      }
    }
  }

private:
  static constexpr size_t kNone = ~size_t(0);
  // A cell holding no slot. Cells hold slot + 1.
  static constexpr uint32_t kEmpty = 0;

  struct Index
  {
    // A power of two cells, each kEmpty or a slot + 1.
    std::vector<uint32_t> table;
    // Cells holding a slot, that slot a hole or not.
    size_t used = 0;
    // Slots in the list emptied by remove, not yet closed.
    size_t holes = 0;
  };

  struct Block
  {
    std::vector<T*> list;
    std::unique_ptr<Index> index;

    void compactList()
    {
      list.erase(std::remove(list.begin(), list.end(), nullptr), list.end());
      index->holes = 0;
    }
  };

  Block* m_block = nullptr;

  static size_t hash(const T* dependent, size_t mask)
  {
    auto bits = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dependent));
    return static_cast<size_t>(((bits >> 3) * 0x9E3779B97F4A7C15ull) >> 32) &
           mask;
  }

  // The dependent's slot, or kNone. When it isn't here and free is given,
  // free is the cell to put it in: the first one along its probe whose slot
  // is a hole, or else the empty cell the probe ended on.
  size_t findSlot(const T* dependent, size_t* free) const
  {
    const auto& list = m_block->list;
    const auto& table = m_block->index->table;
    const size_t mask = table.size() - 1;
    size_t reuse = kNone;
    for (size_t cell = hash(dependent, mask);; cell = (cell + 1) & mask)
    {
      uint32_t entry = table[cell];
      if (entry == kEmpty)
      {
        if (free != nullptr)
        {
          *free = reuse != kNone ? reuse : cell;
        }
        return kNone;
      }
      size_t slot = entry - 1;
      if (list[slot] == dependent)
      {
        return slot;
      }
      if (list[slot] == nullptr && reuse == kNone)
      {
        reuse = cell;
      }
    }
  }

  void compact()
  {
    m_block->compactList();
    afterCompact();
  }

  // A short list goes back to scanning; a long one reindexes its new slots.
  void afterCompact()
  {
    if (m_block->list.size() < kIndexedFrom / 2)
    {
      m_block->index.reset();
      return;
    }
    rebuildIndex();
  }

  void rebuildIndex()
  {
    const auto& list = m_block->list;
    auto index = m_block->index.get();
    size_t cells = 64;
    while (cells < list.size() * 4)
    {
      cells *= 2;
    }
    index->table.assign(cells, kEmpty);
    index->used = 0;
    const size_t mask = cells - 1;
    for (size_t slot = 0; slot < list.size(); slot++)
    {
      if (list[slot] == nullptr)
      {
        continue;
      }
      size_t cell = hash(list[slot], mask);
      while (index->table[cell] != kEmpty)
      {
        cell = (cell + 1) & mask;
      }
      index->table[cell] = static_cast<uint32_t>(slot + 1);
      index->used++;
    }
  }
};
} // namespace rive
#endif
