#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <cmath>

using namespace rive;

void VirtualLayout::beginLinear(float gap)
{
    m_itemCount = m_lineCount = 0;
    m_naturalExtent = -1.0f;
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
                              float columnGap)
{
    m_columnGap = columnGap;
    beginLinear(rowGap);
    m_wraps = true;
    m_grid = true;
    m_columnCount = std::max(1, columnCount);
    m_columnWidths.assign(m_columnCount, 0.0f);
    m_columnStarts.clear();
    for (auto axis : {&m_rows, &m_columns})
    {
        axis->templates.clear();
        axis->autos.clear();
        axis->space = -1.0f;
        axis->resize = false;
    }
    m_columnTrackSizes.clear();
    m_align = align;
}

const VirtualGridTrack& VirtualLayout::trackAt(const VirtualGridAxis& axis,
                                               int index)
{
    static const VirtualGridTrack autoTrack;
    int templateCount = (int)axis.templates.size();
    if (index < templateCount)
    {
        return axis.templates[index];
    }
    if (axis.autos.empty())
    {
        return autoTrack;
    }
    return axis.autos[(index - templateCount) % axis.autos.size()];
}

// A fixed or filling grid sizes its tracks in its space; a hugging one at its
// natural size, with percents as auto, then again in that size, which it
// keeps, when there are percents or the axis resizes.
float VirtualLayout::sizeAxis(const VirtualGridAxis& axis,
                              const std::vector<float>& content,
                              float gap,
                              std::vector<float>& sizes)
{
    sizeTracks(content, axis.space, axis, gap, sizes);
    int n = (int)content.size();
    bool sizesAgain = axis.space < 0.0f && axis.resize;
    for (int index = 0; index < n && axis.space < 0.0f && !sizesAgain; index++)
    {
        auto& track = trackAt(axis, index);
        sizesAgain = track.minSizing == VirtualTrackSizing::percent ||
                     track.maxSizing == VirtualTrackSizing::percent;
    }
    if (!sizesAgain)
    {
        return -1.0f;
    }
    float natural = n > 1 ? gap * (n - 1) : 0.0f;
    for (float size : sizes)
    {
        natural += size;
    }
    sizeTracks(content, natural, axis, gap, sizes);
    return natural;
}

// Sizes grid rows from their tracks and tallest items (see sizeAxis).
void VirtualLayout::sizeGridRows()
{
    // Template rows nothing fills still take their space.
    while (m_lineCount < (int)m_rows.templates.size())
    {
        pushLine(0.0f, 0.0f);
    }
    int n = m_lineCount;
    std::vector<float> content(m_lineExtent.begin(), m_lineExtent.begin() + n);
    std::vector<float> sizes;
    float natural = sizeAxis(m_rows, content, m_gap, sizes);
    if (natural >= 0.0f)
    {
        m_naturalExtent = natural;
    }
    m_running = 0.0f;
    m_trailing = -std::numeric_limits<float>::infinity();
    for (int line = 0; line < n; line++)
    {
        float extent = sizes[line];
        m_lineStart[line] = m_running;
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
        m_trailing = std::max(m_trailing, m_running + extent);
        m_trailingMax[line] = m_trailing;
        m_running += extent + m_gap;
    }
}

// One pass of grid track sizing, for rows or columns: tracks start at their
// minimum (their largest item when that's auto). In a definite space they
// grow toward their limit while there's room and fr tracks share what's
// left; otherwise fr tracks take a size per fr that fits each one. Percents
// are of space, or auto when it's < 0.
void VirtualLayout::sizeTracks(const std::vector<float>& content,
                               float space,
                               const VirtualGridAxis& axis,
                               float gap,
                               std::vector<float>& size)
{
    int n = (int)content.size();
    size.assign(n, 0.0f);
    std::vector<float> limit(n, 0.0f);
    std::vector<float> flex(n, 0.0f);
    auto resolve = [&](VirtualTrackSizing sizing, float value, float autoSize) {
        switch (sizing)
        {
            case VirtualTrackSizing::points:
                return value;
            // A percent of a size that depends on the tracks sizes as auto.
            case VirtualTrackSizing::percent:
                return space >= 0.0f ? value / 100.0f * space : autoSize;
            // fr only flexes as a max; as a min it's auto.
            default:
                return autoSize;
        }
    };
    bool anyFlex = false;
    for (int row = 0; row < n; row++)
    {
        auto& track = trackAt(axis, row);
        size[row] = resolve(track.minSizing, track.minValue, content[row]);
        if (track.maxSizing == VirtualTrackSizing::fr)
        {
            flex[row] = track.maxValue;
            limit[row] = size[row];
            anyFlex = anyFlex || flex[row] > 0.0f;
        }
        else
        {
            limit[row] = std::max(
                size[row],
                resolve(track.maxSizing, track.maxValue, content[row]));
        }
    }
    float gaps = n > 1 ? gap * (n - 1) : 0.0f;
    if (space >= 0.0f)
    {
        // Room left grows rows toward their limits, evenly.
        float room = space - gaps;
        for (float s : size)
        {
            room -= s;
        }
        while (room > 1e-6f)
        {
            int growing = 0;
            for (int row = 0; row < n; row++)
            {
                if (limit[row] > size[row])
                {
                    growing++;
                }
            }
            if (growing == 0)
            {
                break;
            }
            float share = room / growing;
            for (int row = 0; row < n; row++)
            {
                if (limit[row] > size[row])
                {
                    float grow = std::min(share, limit[row] - size[row]);
                    size[row] += grow;
                    room -= grow;
                }
            }
        }
    }
    if (!anyFlex)
    {
        return;
    }
    // Factors under 1 in total count as 1.
    float frSize = 0.0f;
    if (space < 0.0f)
    {
        // Each fr row needs fr enough for its minimum, and for its items
        // unless sharing them would leave it under the minimum, which then
        // only needs what's left over it.
        for (int row = 0; row < n; row++)
        {
            if (flex[row] > 0.0f)
            {
                float factor = std::max(flex[row], 1.0f);
                float items = content[row] / factor;
                frSize = std::max(frSize,
                                  std::max(size[row] / factor,
                                           flex[row] * items < size[row]
                                               ? content[row] - size[row]
                                               : items));
            }
        }
    }
    else
    {
        // Share what the other rows leave; a row whose share is under its
        // minimum keeps the minimum and drops out of the sharing.
        std::vector<uint8_t> fixed(n, 0);
        bool settled = false;
        while (!settled)
        {
            float left = space - gaps;
            float factors = 0.0f;
            for (int row = 0; row < n; row++)
            {
                if (flex[row] > 0.0f && !fixed[row])
                {
                    factors += flex[row];
                }
                else
                {
                    left -= size[row];
                }
            }
            frSize = std::max(0.0f, left) / std::max(factors, 1.0f);
            settled = true;
            for (int row = 0; row < n; row++)
            {
                if (flex[row] > 0.0f && !fixed[row] &&
                    flex[row] * frSize < size[row])
                {
                    fixed[row] = 1;
                    settled = false;
                }
            }
        }
    }
    for (int row = 0; row < n; row++)
    {
        if (flex[row] > 0.0f)
        {
            size[row] = std::max(size[row], flex[row] * frSize);
        }
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
        // Rows size together once all are known (see sizeGridRows); until
        // then a row's extent is its tallest item.
        for (int item = first; item < m_itemCount; item++)
        {
            int column = item - first;
            m_columnWidths[column] =
                std::max(m_columnWidths[column], m_itemFlowExtent[item]);
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
        sizeAxis(m_columns, m_columnWidths, m_columnGap, m_columnTrackSizes);
        sizeGridRows();
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
