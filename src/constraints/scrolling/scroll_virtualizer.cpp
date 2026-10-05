#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/constraints/scrolling/scroll_virtualizer.hpp"
#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <algorithm>
#include <set>

using namespace rive;

ScrollVirtualizer::~ScrollVirtualizer() { reset(); }

void ScrollVirtualizer::reset() { m_anchorItem = -1; }

float ScrollVirtualizer::anchorMoved(
    const VirtualLayout& layout,
    std::vector<LayoutNodeProvider*>& children) const
{
    if (m_anchorItem < 0)
    {
        return 0.0f;
    }
    int item = m_anchorItem;
    if (m_anchorInstance == nullptr)
    {
        // Not a list item: an index only names the same item while the lists
        // are unchanged.
        if (layout.itemCount() != m_anchorItemCount)
        {
            return 0.0f;
        }
    }
    else
    {
        // Follow the instance to wherever a list update moved its item; no
        // anchor if it went away.
        item = -1;
        int segments = std::min((int)children.size(), layout.segmentCount());
        std::vector<int> held;
        for (int segment = 0; segment < segments && item < 0; segment++)
        {
            auto virt =
                VirtualizingComponent::fromScrollChild(children[segment]);
            if (virt == nullptr)
            {
                continue;
            }
            int first = layout.segmentStart(segment);
            int local = m_anchorItem - first;
            if (local >= 0 && local < virt->itemCount() &&
                virt->item(local) == m_anchorInstance)
            {
                item = m_anchorItem;
                break;
            }
            held.clear();
            virt->realizedIndices(held);
            for (int index : held)
            {
                if (virt->item(index) == m_anchorInstance)
                {
                    item = first + index;
                    break;
                }
            }
        }
        if (item < 0 || item >= layout.itemCount())
        {
            return 0.0f;
        }
    }
    // A row layout hasn't placed yet sits at a guess, not where it moved.
    int line = layout.lineOfItem(item);
    if (!layout.lineLaidOut(line))
    {
        return 0.0f;
    }
    return layout.lineStart(line) - m_anchorStart;
}

bool ScrollVirtualizer::constrain(ScrollConstraint* scroll,
                                  std::vector<LayoutNodeProvider*>& children,
                                  float offset,
                                  VirtualizedDirection direction,
                                  float flowOffset,
                                  float columnStart,
                                  float columnViewport,
                                  bool windowsColumns)
{
    m_windowsColumns = windowsColumns;
    m_flowOffset = flowOffset;
    m_columnStart = columnStart;
    m_columnViewport = columnViewport;
    bool isHorz = direction == VirtualizedDirection::horizontal;
    double contentSize =
        isHorz ? scroll->contentWidth() : scroll->contentHeight();
    if (contentSize > 0.0f)
    {
        float normalizedOffset = -offset;
        m_direction = direction;
        m_viewportSize =
            isHorz ? scroll->viewportWidth() : scroll->viewportHeight();
        m_infinite = scroll->infinite();
        if (offset > 0.0f)
        {
            if (m_infinite)
            {
                int offsetMultiplier =
                    static_cast<int>(std::floor(offset / contentSize)) + 1;
                m_offset = -1.0f * (offset - (offsetMultiplier * contentSize));
            }
            else
            {
                m_offset = -offset;
            }
        }
        else
        {
            int offsetMultiplier =
                static_cast<int>(std::floor(normalizedOffset / contentSize));
            m_offset = offsetMultiplier > 0
                           ? std::fmod(normalizedOffset,
                                       offsetMultiplier * contentSize)
                           : normalizedOffset;
        }
        virtualize(scroll, children);
    }
    return true;
}

void ScrollVirtualizer::virtualize(ScrollConstraint* scroll,
                                   std::vector<LayoutNodeProvider*>& children)
{
    auto layout = scroll->virtualLayout();
    if (layout == nullptr || layout->segmentCount() != (int)children.size())
    {
        return;
    }
    bool isHorz = m_direction == VirtualizedDirection::horizontal;
    int totalItemCount = layout->itemCount();

    // Keep `virtualizeBuffer` lines realized on each side of the visible range
    // so items are mounted and advancing before they scroll in. Buffered items
    // are drawn (clipped away by a normal viewport), but stay out of the
    // visible range, which is what reports measured sizes back to us.
    int buffer =
        std::min(static_cast<int>(scroll->virtualizeBuffer()), totalItemCount);
    auto window = layout->window(m_offset, m_viewportSize, m_infinite, buffer);
    // Only realized columns of each realized row exist when columns window.
    bool windowsColumns = m_windowsColumns && layout->isGrid();
    // A carousel scrolled both ways cycles its columns too; whole cycles
    // move nothing, so drop them to keep offsets small.
    bool loopsColumns = windowsColumns && m_infinite;
    float flowOffset = m_flowOffset;
    float columnStart = m_columnStart;
    float columnCycle = layout->columnCycleExtent();
    if (loopsColumns && columnCycle > 0.0f)
    {
        float cycles = std::floor(columnStart / columnCycle) * columnCycle;
        flowOffset -= cycles;
        columnStart -= cycles;
    }
    VirtualWindow columns = windowsColumns
                                ? layout->columnWindow(columnStart,
                                                       m_columnViewport,
                                                       buffer,
                                                       loopsColumns)
                                : VirtualWindow();
    // Where a row's column sits in the (unwrapped) column window.
    auto windowColumn = [&](int line, int item) {
        int column = item - layout->lineFirstItem(line);
        return loopsColumns
                   ? columns.start + layout->wrapColumn(column - columns.start)
                   : column;
    };
    auto inColumns = [&](int line, int item) {
        if (!windowsColumns)
        {
            return true;
        }
        int column = windowColumn(line, item);
        return column >= columns.start && column <= columns.end;
    };

    for (auto child : children)
    {
        auto virt = VirtualizingComponent::fromScrollChild(child);
        if (virt != nullptr)
        {
            virt->clearVirtualWindow();
        }
    }

    // Recycle what the lists hold that this window doesn't. They're asked
    // rather than remembered: a list update moves held items to new indices.
    // Line indices wrap through the layout, so these are list items.
    // Sized to the window, not the list, so a pass costs what it shows.
    std::vector<int> used;
    for (int line = window.start; line <= window.end; line++)
    {
        for (int item = layout->lineFirstItem(line),
                 last = layout->lineLastItem(line);
             item <= last;
             item++)
        {
            if (inColumns(line, item))
            {
                used.push_back(item);
            }
        }
    }
    std::sort(used.begin(), used.end());
    auto isUsed = [&](int item) {
        return std::binary_search(used.begin(), used.end(), item);
    };
    // An item listed more than once shares its instance across its indices:
    // keep it while any of them is used, and recycle it once.
    std::vector<int> held;
    std::vector<Virtualizable*> heldItems;
    // Few items are realized at once, so linear scans beat sets here.
    std::vector<Virtualizable*> kept;
    std::vector<Virtualizable*> recycled;
    auto contains = [](const std::vector<Virtualizable*>& items,
                       Virtualizable* item) {
        return std::find(items.begin(), items.end(), item) != items.end();
    };
    for (int segment = 0; segment < (int)children.size(); segment++)
    {
        auto virt = VirtualizingComponent::fromScrollChild(children[segment]);
        if (virt == nullptr)
        {
            continue;
        }
        held.clear();
        virt->realizedIndices(held);
        int first = layout->segmentStart(segment);
        int count = (segment + 1 < (int)children.size()
                         ? layout->segmentStart(segment + 1)
                         : totalItemCount) -
                    first;
        // Read each index's instance before recycling clears its aliases.
        heldItems.clear();
        kept.clear();
        recycled.clear();
        for (int index : held)
        {
            heldItems.push_back(virt->item(index));
            if (index < count && isUsed(first + index))
            {
                kept.push_back(heldItems.back());
            }
        }
        for (size_t i = 0; i < held.size(); i++)
        {
            int index = held[i];
            if (index < count && isUsed(first + index))
            {
                continue;
            }
            auto shared = heldItems[i];
            if (!contains(kept, shared) && !contains(recycled, shared))
            {
                recycled.push_back(shared);
                virt->removeVirtualizable(index);
            }
        }
    }

    // Carousel offsets wrap, so there's no fixed place to anchor to. Anchor
    // to the first item on screen: in the first line holding one (a grid's
    // empty template rows hold none), and in its first column on screen when
    // columns window.
    m_anchorItem = -1;
    for (int line = window.visibleStart;
         !m_infinite && !window.isEmpty() && line <= window.visibleEnd &&
         m_anchorItem < 0;
         line++)
    {
        if (!layout->lineLaidOut(line))
        {
            break;
        }
        int first = layout->lineFirstItem(line);
        int last = layout->lineLastItem(line);
        int columnsShown =
            windowsColumns ? columns.visibleEnd - columns.visibleStart + 1 : 1;
        for (int shown = 0; shown < columnsShown; shown++)
        {
            int column = 0;
            if (windowsColumns)
            {
                column = columns.visibleStart + shown;
                column = loopsColumns ? layout->wrapColumn(column) : column;
            }
            if (first + column <= last)
            {
                m_anchorItem = first + column;
                m_anchorStart = layout->lineStart(line);
                m_anchorItemCount = totalItemCount;
                break;
            }
        }
    }

    std::set<VirtualizingComponent*> changedVirtualizingComponents;

    std::vector<int> lineItems;
    for (int line = window.start; line <= window.end; line++)
    {
        // Buffered items are realized and drawn, but only on screen items
        // report their measured size back.
        bool isVisible = window.isVisible(line);
        float position = layout->lineStart(line) - m_offset;
        // Items join the window in the order they paint: on screen order, so
        // looping columns that wrap past the last follow it.
        lineItems.clear();
        int first = layout->lineFirstItem(line);
        int last = layout->lineLastItem(line);
        if (loopsColumns)
        {
            for (int column = columns.start; column <= columns.end; column++)
            {
                int item = first + layout->wrapColumn(column);
                if (item <= last)
                {
                    lineItems.push_back(item);
                }
            }
        }
        else
        {
            for (int item = first; item <= last; item++)
            {
                if (inColumns(line, item))
                {
                    lineItems.push_back(item);
                }
            }
        }
        for (int item : lineItems)
        {
            int segment = layout->segmentOf(item);
            auto virt =
                VirtualizingComponent::fromScrollChild(children[segment]);
            if (virt == nullptr)
            {
                continue;
            }
            int childIndex = item - layout->segmentStart(segment);
            bool itemVisible =
                isVisible && (!windowsColumns ||
                              columns.isVisible(windowColumn(line, item)));
            virt->addToVirtualWindow(childIndex, itemVisible);
            if (virt->item(childIndex) == nullptr)
            {
                virt->addVirtualizable(childIndex);
                changedVirtualizingComponents.emplace(virt);
            }
            // Layout holds every row, so a realized item takes its own cell
            // rather than the next free one. Unpin once a pass stops pinning.
            if (layout->isGrid() || m_pinnedCells)
            {
                bool grid = layout->isGrid();
                virt->setVirtualizableCell(
                    childIndex,
                    grid ? item - layout->lineFirstItem(line) : -1,
                    grid ? layout->wrapLine(line) : -1);
            }

            auto virtualizable = virt->item(childIndex);
            if (virtualizable == nullptr)
            {
                continue;
            }
            auto virtualizableComponent =
                virtualizable->virtualizableComponent();
            if (virtualizableComponent == nullptr ||
                !virtualizableComponent->is<ArtboardInstance>())
            {
                continue;
            }
            Mat2D inverse;
            if (!children[segment]
                     ->transformComponent()
                     ->worldTransform()
                     .invert(&inverse))
            {
                continue;
            }
            auto artboardInstance =
                virtualizableComponent->as<ArtboardInstance>();
            // Layout already placed a realized item along its line when it saw
            // the same lines we did (see VirtualLayout::flowFromLayout).
            float flow = !layout->flowFromLayout()
                             ? layout->itemFlowOffset(item)
                         : isHorz ? artboardInstance->layoutY()
                                  : artboardInstance->layoutX();
            // Lines move along the flow axis when it scrolls too, and a
            // cycling column moves to the cycle it's realized in.
            flow -= flowOffset;
            if (loopsColumns)
            {
                int column = item - layout->lineFirstItem(line);
                flow += layout->columnStart(windowColumn(line, item)) -
                        layout->columnStart(column);
            }
            float scroll = position + layout->itemLineOffset(item);
            auto location = isHorz ? Vec2D(scroll, flow) : Vec2D(flow, scroll);
            virt->setVirtualizablePosition(childIndex, location);
        }
    }

    // The anchor was realized above; remember its instance to follow it.
    m_anchorInstance = nullptr;
    if (m_anchorItem >= 0)
    {
        int segment = layout->segmentOf(m_anchorItem);
        auto virt = VirtualizingComponent::fromScrollChild(children[segment]);
        if (virt != nullptr)
        {
            m_anchorInstance =
                virt->item(m_anchorItem - layout->segmentStart(segment));
        }
    }
    m_pinnedCells = layout->isGrid();
    for (auto& virtualizingComponent : changedVirtualizingComponents)
    {
        virtualizingComponent->virtualizableChanged();
    }
}
