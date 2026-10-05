#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <cmath>

using namespace rive;

void VirtualLayout::beginLinear(float gap)
{
    m_itemCount = m_lineCount = 0;
    m_gap = gap;
    m_running = 0.0f;
    m_trailing = -std::numeric_limits<float>::infinity();
    m_lineOpen = false;
    m_wraps = false;
    m_grid = false;
    m_hugsLines = false;
    m_flowFromLayout = true;
    m_lineUsed.clear();
    m_lineStart.clear();
    m_lineExtent.clear();
    m_trailingMax.clear();
    m_lineFirstItem.clear();
    m_segmentStart.clear();
    m_itemExtent.clear();
    m_itemFlowExtent.clear();
    m_itemFlowOffset.clear();
    m_itemLineOffset.clear();
}

void VirtualLayout::beginWrap(float lineGap,
                              float flowGap,
                              float flowStart,
                              float flowExtent,
                              LayoutMainDistribute justify,
                              LayoutCrossAlign align,
                              bool hugsLines)
{
    beginLinear(lineGap);
    m_wraps = true;
    m_hugsLines = hugsLines;
    m_flowGap = flowGap;
    m_flowStart = flowStart;
    m_flowExtent = flowExtent;
    m_justify = justify;
    m_align = align;
}

void VirtualLayout::beginGrid(float rowGap,
                              int columnCount,
                              LayoutCrossAlign align,
                              float columnGap,
                              int rowCount)
{
    m_columnGap = columnGap;
    beginLinear(rowGap);
    m_wraps = true;
    m_grid = true;
    m_columnCount = std::max(1, columnCount);
    m_rowCount = rowCount;
    m_columnContents.assign(m_columnCount, 0.0f);
    m_columnStarts.clear();
    m_rowStarts.clear();
    m_align = align;
}

// Rows sit on layout's lines once it has sized every track from the row and
// column contents; rows it hasn't laid out yet stack at their content size.
void VirtualLayout::placeGridRows()
{
    // Template rows nothing fills still take their space.
    while (m_lineCount < m_rowCount)
    {
        pushLine(0.0f, 0.0f);
    }
    int n = m_lineCount;
    m_rowContents.assign(m_lineExtent.begin(), m_lineExtent.begin() + n);
    int lines = (int)m_rowStarts.size();
    m_running = 0.0f;
    m_trailing = -std::numeric_limits<float>::infinity();
    for (int line = 0; line < n; line++)
    {
        float start = m_running;
        float extent = m_rowContents[line];
        if (line + 1 < lines)
        {
            // The last line closes the last track; the others open one after
            // a gap.
            start = m_rowStarts[line] - m_rowStarts[0];
            extent = m_rowStarts[line + 1] - m_rowStarts[line] -
                     (line + 2 < lines ? m_gap : 0.0f);
        }
        m_lineStart[line] = start;
        m_lineExtent[line] = extent;
        int last = line + 1 < n ? m_lineFirstItem[line + 1] : m_itemCount;
        for (int item = m_lineFirstItem[line]; item < last; item++)
        {
            // Yoga's grid centers an item in its row but reads the FlexEnd
            // that alignment writes as start; match it.
            float slack = extent - m_itemExtent[item];
            m_itemLineOffset[item] =
                m_align == LayoutCrossAlign::center ? slack / 2.0f : 0.0f;
        }
        m_trailing = std::max(m_trailing, start + extent);
        m_trailingMax[line] = m_trailing;
        m_running = start + extent + m_gap;
    }
}

void VirtualLayout::beginSegment() { m_segmentStart.push_back(m_itemCount); }

void VirtualLayout::pushLine(float start, float extent)
{
    m_lineStart.push_back(start);
    m_lineExtent.push_back(extent);
    m_lineFirstItem.push_back(m_itemCount);
    m_trailingMax.push_back(0.0f);
    if (m_wraps)
    {
        m_lineUsed.push_back(0.0f);
    }
    m_lineCount++;
}

void VirtualLayout::openLine()
{
    pushLine(m_running, 0.0f);
    m_lineOpen = true;
    m_lineFlowUsed = 0.0f;
}

void VirtualLayout::closeLine()
{
    int line = m_lineCount - 1;
    int first = m_lineFirstItem[line];
    if (m_grid)
    {
        // Rows are placed once all are known (see placeGridRows); until
        // then a row's extent is its tallest item.
        for (int item = first; item < m_itemCount; item++)
        {
            int column = item - first;
            m_columnContents[column] =
                std::max(m_columnContents[column], m_itemFlowExtent[item]);
            m_itemFlowOffset[item] = column < (int)m_columnStarts.size()
                                         ? m_columnStarts[column]
                                         : 0.0f;
        }
        m_lineOpen = false;
        return;
    }
    float lineExtent = m_lineExtent[line];

    m_lineUsed[line] = m_lineFlowUsed;
    for (int item = first; item < m_itemCount; item++)
    {
        float slack = lineExtent - m_itemExtent[item];
        m_itemLineOffset[item] = m_align == LayoutCrossAlign::start ? 0.0f
                                 : m_align == LayoutCrossAlign::center
                                     ? slack / 2.0f
                                     : slack;
    }
    // Hugging content is as wide as its widest line, known only at end().
    if (!m_hugsLines)
    {
        placeLine(line, m_flowExtent);
    }

    m_trailing = std::max(m_trailing, m_lineStart[line] + lineExtent);
    m_trailingMax[line] = m_trailing;
    m_running = m_lineStart[line] + lineExtent + m_gap;
    m_lineOpen = false;
}

void VirtualLayout::placeLine(int line, float extent)
{
    int first = m_lineFirstItem[line];
    int last = line + 1 < (int)m_lineFirstItem.size() && line + 1 < m_lineCount
                   ? m_lineFirstItem[line + 1]
                   : m_itemCount;
    int count = last - first;
    float freeSpace = extent - m_lineUsed[line];
    float lead = 0.0f;
    float between = m_flowGap;
    switch (m_justify)
    {
        case LayoutMainDistribute::start:
            break;
        case LayoutMainDistribute::center:
            lead = freeSpace / 2.0f;
            break;
        case LayoutMainDistribute::end:
            lead = freeSpace;
            break;
        case LayoutMainDistribute::spaceBetween:
            if (count > 1)
            {
                between += std::max(freeSpace, 0.0f) / (count - 1);
            }
            break;
    }
    float flow = m_flowStart + lead;
    for (int item = first; item < last; item++)
    {
        m_itemFlowOffset[item] = flow;
        flow += m_itemFlowExtent[item] + between;
    }
}

void VirtualLayout::addItem(float extent, float flowExtent)
{
    if (!m_wraps)
    {
        pushLine(m_running, extent);
        m_trailing = std::max(m_trailing, m_running + extent);
        m_trailingMax.back() = m_trailing;
        m_running += extent + m_gap;
        m_itemCount++;
        return;
    }
    // Flex wrap breaks before an item that would overflow a non-empty line;
    // a grid row breaks once its columns are full.
    if (m_lineOpen &&
        (m_grid
             ? m_itemCount - m_lineFirstItem[m_lineCount - 1] >= m_columnCount
             : m_lineFlowUsed + m_flowGap + flowExtent > m_flowExtent))
    {
        closeLine();
    }
    if (!m_lineOpen)
    {
        openLine();
        m_lineFlowUsed = flowExtent;
    }
    else
    {
        m_lineFlowUsed += m_flowGap + flowExtent;
    }
    int line = m_lineCount - 1;
    m_lineExtent[line] = std::max(m_lineExtent[line], extent);
    m_itemExtent.push_back(extent);
    m_itemFlowExtent.push_back(flowExtent);
    m_itemFlowOffset.push_back(0.0f);
    m_itemLineOffset.push_back(0.0f);
    m_itemCount++;
}

void VirtualLayout::end()
{
    if (m_lineOpen)
    {
        closeLine();
    }
    if (m_grid)
    {
        placeGridRows();
    }
    m_lineFirstItem.push_back(m_itemCount);
    m_startSuffixMin.resize(m_lineCount);
    // Gaps more negative than a line is long start later lines before
    // earlier ones; the lowest start from each line on finds the last line
    // starting before a point anyway.
    float lowest = std::numeric_limits<float>::infinity();
    for (int line = m_lineCount - 1; line >= 0; line--)
    {
        lowest = std::min(lowest, m_lineStart[line]);
        m_startSuffixMin[line] = lowest;
    }
    if (m_wraps && !m_grid && m_hugsLines)
    {
        float widest = 0.0f;
        for (int line = 0; line < m_lineCount; line++)
        {
            widest = std::max(widest, m_lineUsed[line]);
        }
        for (int line = 0; line < m_lineCount; line++)
        {
            placeLine(line, widest);
        }
    }
}

float VirtualLayout::itemFlowOffset(int item) const
{
    return m_wraps ? m_itemFlowOffset[item] : 0.0f;
}

float VirtualLayout::itemLineOffset(int item) const
{
    return m_wraps ? m_itemLineOffset[item] : 0.0f;
}

int VirtualLayout::segmentOf(int item) const
{
    // Last segment starting at or before the item; empty segments share a
    // start with the next one, so take the last match.
    auto it =
        std::upper_bound(m_segmentStart.begin(), m_segmentStart.end(), item);
    return (int)(it - m_segmentStart.begin()) - 1;
}

float VirtualLayout::lineStart(int line) const
{
    int wrapped = wrapLine(line);
    int cycle = (line - wrapped) / m_lineCount;
    return cycle * cycleExtent() + m_lineStart[wrapped];
}

float VirtualLayout::lineExtent(int line) const
{
    return m_lineExtent[wrapLine(line)];
}

int VirtualLayout::lineFirstItem(int line) const
{
    return m_lineFirstItem[wrapLine(line)];
}

int VirtualLayout::lineLastItem(int line) const
{
    return m_lineFirstItem[wrapLine(line) + 1] - 1;
}

int VirtualLayout::lineOfItem(int item) const
{
    int lo = 0, hi = m_lineCount - 1;
    while (lo < hi)
    {
        int mid = (lo + hi + 1) >> 1;
        if (m_lineFirstItem[mid] <= item)
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return lo;
}

int VirtualLayout::lastStartBefore(float value) const
{
    // The lowest start from each line on only grows, so the first line from
    // which every start is at or past value bounds the answer.
    int lo = 0, hi = m_lineCount;
    while (lo < hi)
    {
        int mid = (lo + hi) >> 1;
        if (m_startSuffixMin[mid] >= value)
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return lo - 1;
}

int VirtualLayout::firstTrailingPast(float value, bool inclusive) const
{
    int lo = 0, hi = m_lineCount;
    while (lo < hi)
    {
        int mid = (lo + hi) >> 1;
        float edge = m_trailingMax[mid];
        if (edge > value || (inclusive && edge == value))
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return lo;
}

VirtualWindow VirtualLayout::window(float offset,
                                    float viewport,
                                    bool infinite,
                                    int buffer) const
{
    int n = m_lineCount;
    if (n == 0)
    {
        return VirtualWindow();
    }
    // First line still on screen: its trailing edge is past the offset.
    int start = firstTrailingPast(offset);
    if (start == n)
    {
        // Only a carousel's trailing gap can be past every line; the next
        // cycle's first line is the one coming in.
        start = infinite ? n : n - 1;
    }

    // Last line on screen: the last one starting before the far edge.
    float limit = offset + viewport;
    int end;
    if (!infinite)
    {
        end = std::max(start, lastStartBefore(limit));
    }
    else
    {
        // The latest unwrapped line starting before it, at most a cycle on.
        int latest = -1;
        float cycle = cycleExtent();
        for (int c = 0; c <= 2; c++)
        {
            int found = lastStartBefore(limit - c * cycle);
            if (found >= 0)
            {
                latest = c * n + found;
            }
        }
        end = std::min(std::max(start, latest), start + n - 1);
    }

    VirtualWindow window;
    window.visibleStart = start;
    window.visibleEnd = end;
    bufferWindow(window, n, buffer, infinite);
    return window;
}

void VirtualLayout::bufferWindow(VirtualWindow& window,
                                 int count,
                                 int buffer,
                                 bool cycles)
{
    int start = window.visibleStart;
    int end = window.visibleEnd;
    window.start = start;
    window.end = end;
    if (buffer <= 0)
    {
        return;
    }
    int extra = std::max(0, count - (end - start + 1));
    int before = std::max(0, std::min(buffer, cycles ? extra : start));
    int after =
        std::max(0,
                 std::min(buffer, cycles ? extra - before : count - 1 - end));
    window.start = start - before;
    window.end = end + after;
}

float VirtualLayout::columnCycleExtent() const
{
    int count = columnCount();
    if (!m_grid || (int)m_columnStarts.size() < count + 1)
    {
        return 0.0f;
    }
    return m_columnStarts[count] - m_columnStarts[0] + m_columnGap;
}

float VirtualLayout::columnStart(int column) const
{
    int wrapped = wrapColumn(column);
    if (wrapped >= (int)m_columnStarts.size())
    {
        return 0.0f;
    }
    int cycle = (column - wrapped) / columnCount();
    return cycle * columnCycleExtent() + m_columnStarts[wrapped];
}

VirtualWindow VirtualLayout::columnWindow(float offset,
                                          float viewport,
                                          int buffer,
                                          bool infinite) const
{
    VirtualWindow window;
    int count = columnCount();
    window.start = window.visibleStart = 0;
    window.end = window.visibleEnd = count - 1;
    // Column lines bound each column: count + 1 of them, gaps included.
    if (!m_grid || (int)m_columnStarts.size() < count + 1)
    {
        return window;
    }
    auto columnEnd = [&](int column) {
        return column < count - 1 ? m_columnStarts[column + 1] - m_columnGap
                                  : m_columnStarts[count];
    };
    float cycle = infinite ? columnCycleExtent() : 0.0f;
    if (cycle <= 0.0f)
    {
        // As with lines: the first column whose trailing edge is past the
        // offset, through the last that starts before the far edge.
        int start = 0;
        while (start < count - 1 && columnEnd(start) <= offset)
        {
            start++;
        }
        int end = start;
        while (end < count - 1 && m_columnStarts[end + 1] < offset + viewport)
        {
            end++;
        }
        window.visibleStart = start;
        window.visibleEnd = end;
        bufferWindow(window, count, buffer, false);
        return window;
    }
    // Find the first column within the offset's cycle, then count on in
    // unwrapped columns; past the last column the next cycle's first is in.
    int k = (int)std::floor((offset - m_columnStarts[0]) / cycle);
    float local = offset - k * cycle;
    int start = 0;
    while (start < count && columnEnd(start) <= local)
    {
        start++;
    }
    start += k * count;
    // At most one cycle: each item is realized once.
    int end = start;
    while (end < start + count - 1 && columnStart(end + 1) < offset + viewport)
    {
        end++;
    }
    window.visibleStart = start;
    window.visibleEnd = end;
    bufferWindow(window, count, buffer, true);
    return window;
}
