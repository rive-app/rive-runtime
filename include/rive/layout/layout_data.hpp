#ifndef _RIVE_LAYOUT_DATA_HPP_
#define _RIVE_LAYOUT_DATA_HPP_

#ifdef WITH_RIVE_LAYOUT
#include "rive/layout/layout_style_applier.hpp"
#include "yoga/YGNode.h"
#include "yoga/YGStyle.h"
#include "yoga/Yoga.h"
#include <algorithm>
#include <cstdint>
#include <vector>
#endif
#ifdef WITH_RIVE_TOOLS
#include "rive/refcnt.hpp"
#include <atomic>
#include <unordered_set>
#endif

namespace rive
{
#ifdef WITH_RIVE_LAYOUT
/// The appliers of one layout item: the item itself, its style and its grid
/// placement. Up to three live inline, so an item allocates nothing for them;
/// a fourth moves the list to the heap.
class LayoutStyleApplierList
{
public:
  LayoutStyleApplierList() = default;
  ~LayoutStyleApplierList()
  {
    if (spilled())
    {
      delete m_spill;
    }
  }
  LayoutStyleApplierList(const LayoutStyleApplierList&) = delete;
  LayoutStyleApplierList& operator=(const LayoutStyleApplierList&) = delete;

  bool empty() const { return m_size == 0; }
  std::size_t size() const { return m_size; }
  LayoutStyleApplier* const* begin() const
  {
    return spilled() ? m_spill->data() : m_inline;
  }
  LayoutStyleApplier* const* end() const { return begin() + m_size; }

  void pushUnique(LayoutStyleApplier* applier)
  {
    if (std::find(begin(), end(), applier) != end())
    {
      return;
    }
    if (m_size < kInline)
    {
      m_inline[m_size++] = applier;
      return;
    }
    if (m_size == kInline)
    {
      m_spill = new std::vector<LayoutStyleApplier*>(
          m_inline,
          m_inline + kInline);
    }
    m_spill->push_back(applier);
    m_size++;
  }

private:
  static constexpr uint32_t kInline = 3;
  bool spilled() const { return m_size > kInline; }

  union
  {
    LayoutStyleApplier* m_inline[kInline] = {};
    std::vector<LayoutStyleApplier*>* m_spill;
  };
  uint32_t m_size = 0;
};
#endif

class LayoutData
#ifdef WITH_RIVE_TOOLS
    : public RefCnt<LayoutData>
#endif
{
public:
#ifdef WITH_RIVE_LAYOUT

#ifdef WITH_RIVE_TOOLS
  std::unordered_set<LayoutData*> children;
#ifdef DEBUG
  LayoutData() { count++; }
#endif
  ~LayoutData()
  {
#ifdef DEBUG
    count--;
#endif
    clearChildren();
  }
  void clearChildren()
  {
    for (auto child : children)
    {
      child->unref();
    }
    children.clear();
  }
#ifdef DEBUG
  static std::atomic<uint32_t> count;
#endif
#endif

  YGNode node;

  /// Lays out in fractional pixels. Every node shares Yoga's default config,
  /// and files load on any thread, so it is set once.
  static void useFractionalPixels()
  {
    static const bool set = []() {
      YGConfigGetDefault()->setPointScaleFactor(0);
      return true;
    }();
    (void)set;
  }

  /// The Yoga style lives inside the node itself; this is the single source
  /// of truth appliers write into. Go through here rather than reaching for
  /// node.getStyle() so callers don't depend on YGNode's accessor shape.
  YGStyle& style() { return node.getStyle(); }
  const YGStyle& style() const { return node.getStyle(); }

  /// Objects contributing to this item's style. Unsorted — apply order comes
  /// from the phase methods, not from list order, which follows file order.
  LayoutStyleApplierList appliers;

  /// No matching remove: runtime objects live for the artboard's lifetime.
  void addApplier(LayoutStyleApplier* applier) { appliers.pushUnique(applier); }

  /// Runs every applier phase by phase, so apply order can't depend on child
  /// order.
  void applyLayoutStyles(YGStyle& style, const LayoutSyncContext& context)
  {
    if (appliers.empty())
    {
      return;
    }
    for (auto* applier : appliers)
    {
      applier->applyBaseStyle(style, context);
    }
    for (auto* applier : appliers)
    {
      applier->applyContainerStyle(style, context);
    }
    for (auto* applier : appliers)
    {
      applier->applyItemStyle(style, context);
    }
    for (auto* applier : appliers)
    {
      applier->applyPlacementStyle(style, context);
    }
  }
#endif
};

#ifdef WITH_RIVE_TOOLS
typedef rcp<LayoutData> LayoutDataRef;
#else
typedef LayoutData* LayoutDataRef;
#endif

} // namespace rive
#endif