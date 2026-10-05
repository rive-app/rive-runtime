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

// How a grid row track sizes, in the order GridTrackSizeType numbers them.
enum class VirtualTrackSizing : uint8_t
{
    autoSize,
    points,
    percent,
    fr,
};

// A grid row track: the smallest it may be and the size it grows to, as
// minmax(min, max). A bare size is both. Percents resolve against the grid's
// height; fr only flexes as a max.
struct VirtualGridTrack
{
    VirtualTrackSizing minSizing = VirtualTrackSizing::autoSize;
    float minValue = 0.0f;
    VirtualTrackSizing maxSizing = VirtualTrackSizing::autoSize;
    float maxValue = 0.0f;

    static VirtualGridTrack points(float value)
    {
        return {VirtualTrackSizing::points,
                value,
                VirtualTrackSizing::points,
                value};
    }
    static VirtualGridTrack autoSize() { return {}; }
    static VirtualGridTrack percent(float value)
    {
        return {VirtualTrackSizing::percent,
                value,
                VirtualTrackSizing::percent,
                value};
    }
    static VirtualGridTrack fr(float value)
    {
        return {VirtualTrackSizing::autoSize,
                0.0f,
                VirtualTrackSizing::fr,
                value};
    }
};

// One grid axis: its tracks and the space layout sizes them in.
struct VirtualGridAxis
{
    std::vector<VirtualGridTrack> templates;
    // Cycle once the templates run out; tracks past both size to their items.
    std::vector<VirtualGridTrack> autos;
    // The grid's inner size along the axis when it's fixed or fills; < 0 when
    // it hugs its tracks.
    float space = -1.0f;
    // Whether layout sizes hugging tracks again in the size they give the
    // grid, as a flex parent does along its main axis.
    bool resize = false;
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

    // Extent along the scroll axis, without padding or a trailing gap. A
    // hugging grid is as tall as its rows were before percents resolved
    // against that height, as layout sizes it.
    float extent() const
    {
        if (m_naturalExtent >= 0.0f)
        {
            return m_naturalExtent;
        }
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
    // Rebuild as grid rows: items fill columnCount cells per row, left to
    // right. Fill gridColumnStarts (may stay empty) and both grid axes after
    // this; they keep their capacity across rebuilds.
    void beginGrid(float rowGap,
                   int columnCount,
                   LayoutCrossAlign align,
                   float columnGap = 0.0f);
    std::vector<float>& gridColumnStarts() { return m_columnStarts; }
    VirtualGridAxis& gridRows() { return m_rows; }
    VirtualGridAxis& gridColumns() { return m_columns; }
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
    // A grid column's width as layout sizes it from its track and every item
    // in it, realized or not.
    float gridColumnSize(int column) const
    {
        return column < (int)m_columnTrackSizes.size()
                   ? m_columnTrackSizes[column]
                   : 0.0f;
    }
    // Track sizes for layout to use over the authored ones (see
    // LayoutComponent::virtualGridTracks); they keep their capacity.
    std::vector<float>& gridColumnSizes() { return m_columnSizes; }
    std::vector<float>& gridRowSizes() { return m_rowSizes; }
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
    // A row's or column's track: templates first, then autos cycling, then
    // auto.
    static const VirtualGridTrack& trackAt(const VirtualGridAxis& axis,
                                           int index);
    // Sizes grid rows the way layout does (see end()).
    void sizeGridRows();
    // Sizes an axis's tracks from their largest items; returns the natural
    // size a hugging grid takes when it sized them again in it, else -1.
    static float sizeAxis(const VirtualGridAxis& axis,
                          const std::vector<float>& content,
                          float gap,
                          std::vector<float>& sizes);
    // One pass of grid track sizing into sizes.
    static void sizeTracks(const std::vector<float>& content,
                           float space,
                           const VirtualGridAxis& axis,
                           float gap,
                           std::vector<float>& sizes);

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
    std::vector<float> m_columnStarts;
    std::vector<float> m_columnWidths;
    std::vector<float> m_columnSizes;
    std::vector<float> m_rowSizes;
    VirtualGridAxis m_rows;
    VirtualGridAxis m_columns;
    std::vector<float> m_columnTrackSizes;
    float m_naturalExtent = -1.0f;
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
