#ifndef _RIVE_VIRTUAL_LAYOUT_HPP_
#define _RIVE_VIRTUAL_LAYOUT_HPP_
#include "rive/layout/layout_enums.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace rive
{
// Lines realized for a viewport, in unwrapped line space: a carousel's lines
// run past lineCount into the next cycle and below 0 into the previous one.
// start..end is realized (buffer included); visibleStart..visibleEnd is on
// screen.
struct VirtualWindow
{
    int start = 0;
    int end = -1;
    int visibleStart = 0;
    int visibleEnd = -1;

    bool isEmpty() const { return end < start; }
    bool isVisible(int line) const
    {
        return line >= visibleStart && line <= visibleEnd;
    }
};

// Items of a virtualized list grouped into lines stacked along the scroll
// axis. A non-wrapping list is the one-item-per-line case; a wrapping one
// flows items along each line (the flow axis).
class VirtualLayout
{
public:
    int itemCount() const { return m_itemCount; }
    int lineCount() const { return m_lineCount; }
    float gap() const { return m_gap; }

    // Extent along the scroll axis, without padding or a trailing gap.
    float extent() const
    {
        return m_lineCount == 0 ? 0.0f
                                : m_lineStart[m_lineCount - 1] +
                                      m_lineExtent[m_lineCount - 1];
    }

    // Distance between a line and its copy in the next carousel cycle.
    float cycleExtent() const { return extent() + m_gap; }

    // Rebuild with one item per line. Items are added in order, grouped into
    // segments (one per scroll child), then end() seals the tables.
    void beginLinear(float gap);
    // Rebuild with items flowing into lines flowExtent long, broken the way
    // flex wrap breaks them. lineGap separates lines, flowGap items in a
    // line; flowStart is where lines begin on the flow axis. When the content
    // hugs its lines, they justify within the widest one instead.
    void beginWrap(float lineGap,
                   float flowGap,
                   float flowStart,
                   float flowExtent,
                   LayoutMainDistribute justify,
                   LayoutCrossAlign align,
                   bool hugsLines = false);
    // Rebuild as grid rows of columnCount cells, at least rowCount rows. Fill
    // gridColumnStarts and gridRowStarts with layout's lines after this.
    void beginGrid(float rowGap,
                   int columnCount,
                   LayoutCrossAlign align,
                   float columnGap = 0.0f,
                   int rowCount = 0);
    std::vector<float>& gridColumnStarts() { return m_columnStarts; }
    std::vector<float>& gridRowStarts() { return m_rowStarts; }
    // The largest item in each row and column, which layout sizes the grid's
    // tracks from (see LayoutComponent::virtualGridContributions).
    const std::vector<float>& gridRowContents() const { return m_rowContents; }
    const std::vector<float>& gridColumnContents() const
    {
        return m_columnContents;
    }
    void beginSegment();
    // extent is the item's size along the scroll axis, flowExtent along its
    // line (only read when wrapping).
    void addItem(float extent, float flowExtent = 0.0f);
    void end();

    bool wraps() const { return m_wraps; }
    // Whether realized items take their flow position from layout (exact,
    // when layout sees whole lines the way we do) rather than from us.
    bool flowFromLayout() const { return m_flowFromLayout; }
    void flowFromLayout(bool value) { m_flowFromLayout = value; }
    bool isGrid() const { return m_grid; }
    int columnCount() const { return m_grid ? m_columnCount : 1; }
    // Grid columns to realize when the grid also scrolls along its rows, with
    // offset and viewport in column-line space; every column when the lines
    // aren't known. Infinite columns cycle like a carousel's lines, in
    // unwrapped column space.
    VirtualWindow columnWindow(float offset,
                               float viewport,
                               int buffer,
                               bool infinite = false) const;
    // Distance between a grid column and its copy in the next cycle; 0 when
    // the column lines aren't known.
    float columnCycleExtent() const;
    // Start of an unwrapped grid column along the flow axis.
    float columnStart(int column) const;
    // The column an unwrapped one repeats.
    int wrapColumn(int column) const
    {
        return wrapIndex(column, columnCount());
    }
    // The line an unwrapped one repeats.
    int wrapLine(int line) const { return wrapIndex(line, m_lineCount); }
    // Whether layout placed a line; a grid row past its lines stacks at its
    // content size until they reach it.
    bool lineLaidOut(int line) const
    {
        return !m_grid || wrapLine(line) + 1 < (int)m_rowStarts.size();
    }
    bool rowsLaidOut() const
    {
        return !m_grid || (int)m_rowStarts.size() > m_lineCount;
    }
    // Where an item sits along the flow axis; 0 when items don't share lines.
    float itemFlowOffset(int item) const;
    // Where an item sits past its line's start along the scroll axis.
    float itemLineOffset(int item) const;

    // One segment of count items; extentAt(i) is an item's scroll-axis size.
    template <typename F> void buildLinear(int count, F extentAt, float gap)
    {
        beginLinear(gap);
        beginSegment();
        for (int i = 0; i < count; i++)
        {
            addItem(extentAt(i));
        }
        end();
    }

    int segmentCount() const { return (int)m_segmentStart.size(); }
    // First global item of a segment.
    int segmentStart(int segment) const { return m_segmentStart[segment]; }
    // Segment holding a global item.
    int segmentOf(int item) const;

    // Start of an unwrapped line along the scroll axis.
    float lineStart(int line) const;
    float lineExtent(int line) const;
    int lineFirstItem(int line) const;
    int lineLastItem(int line) const;
    // Line holding item (wrapped space).
    int lineOfItem(int item) const;

    // Lines to realize with the scroll axis at offset (normalized into
    // [0, cycleExtent) when infinite) and viewport long, plus buffer lines
    // either side.
    VirtualWindow window(float offset,
                         float viewport,
                         bool infinite,
                         int buffer) const;

private:
    // First line in [0, lineCount) whose trailing edge is past value (or
    // reaches it when inclusive), or lineCount when none is.
    int firstTrailingPast(float value, bool inclusive = false) const;
    // Last line in [0, lineCount) starting before value, or -1.
    int lastStartBefore(float value) const;
    // index in [0, count); C++ % keeps the sign of negative indices.
    static int wrapIndex(int index, int count)
    {
        int wrapped = index % count;
        return wrapped < 0 ? wrapped + count : wrapped;
    }
    // Widens a window's visible span by buffer either side, within count;
    // a cycling window spends at most one cycle.
    static void bufferWindow(VirtualWindow& window,
                             int count,
                             int buffer,
                             bool cycles);
    void pushLine(float start, float extent);
    void openLine();
    void closeLine();
    // Spreads a wrapped line's items along extent, as justify-content.
    void placeLine(int line, float extent);
    // Places grid rows on layout's lines (see end()).
    void placeGridRows();

    int m_itemCount = 0;
    int m_lineCount = 0;
    float m_gap = 0.0f;
    // Build state.
    float m_running = 0.0f;
    float m_trailing = 0.0f;
    bool m_lineOpen = false;
    float m_lineFlowUsed = 0.0f;
    std::vector<int32_t> m_segmentStart;
    // Wrap configuration and per-item placement; empty when linear.
    bool m_wraps = false;
    bool m_hugsLines = false;
    bool m_flowFromLayout = true;
    // Flow space each wrapped line's items and gaps use.
    std::vector<float> m_lineUsed;
    float m_flowGap = 0.0f;
    float m_flowStart = 0.0f;
    float m_flowExtent = 0.0f;
    LayoutMainDistribute m_justify = LayoutMainDistribute::start;
    LayoutCrossAlign m_align = LayoutCrossAlign::start;
    bool m_grid = false;
    int m_columnCount = 1;
    float m_columnGap = 0.0f;
    int m_rowCount = 0;
    std::vector<float> m_columnStarts;
    std::vector<float> m_rowStarts;
    std::vector<float> m_columnContents;
    std::vector<float> m_rowContents;
    std::vector<float> m_itemExtent;
    std::vector<float> m_itemFlowExtent;
    std::vector<float> m_itemFlowOffset;
    std::vector<float> m_itemLineOffset;
    std::vector<float> m_lineStart;
    std::vector<float> m_lineExtent;
    // Running max of line trailing edges, so overlapping lines (negative
    // gaps) can still be binary searched.
    std::vector<float> m_trailingMax;
    // Lowest start of any line from each on, for lines that start before
    // earlier ones.
    std::vector<float> m_startSuffixMin;
    // lineCount + 1 entries; line l holds items
    // [m_lineFirstItem[l], m_lineFirstItem[l + 1]).
    std::vector<int32_t> m_lineFirstItem = {0};
};
} // namespace rive

#endif
