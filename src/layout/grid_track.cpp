#include "rive/layout/grid_track.hpp"
#include "rive/layout_component.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "rive/layout/layout_data.hpp"
#endif
#include <cmath>

using namespace rive;

void GridTrack::markLayoutDirty()
{
#ifdef WITH_RIVE_LAYOUT
    if (parent() != nullptr && parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markLayoutNodeDirty();
    }
#endif
}

void GridTrack::collectionChanged() { markLayoutDirty(); }
void GridTrack::trackTypeChanged() { markLayoutDirty(); }
void GridTrack::trackValueChanged() { markLayoutDirty(); }
void GridTrack::trackMaxTypeChanged() { markLayoutDirty(); }
void GridTrack::trackMaxValueChanged() { markLayoutDirty(); }

#ifdef WITH_RIVE_LAYOUT
namespace
{
YGStyleSizeLength gridSizeLength(GridTrackSizeType type, float value)
{
    switch (type)
    {
        case GridTrackSizeType::points:
            return YGStyleSizeLength::points(value);
        case GridTrackSizeType::percent:
            return YGStyleSizeLength::percent(value);
        case GridTrackSizeType::fr:
            return YGStyleSizeLength::stretch(value);
        default:
            return YGStyleSizeLength::ofAuto();
    }
}

YGGridTrackSize gridTrackSize(const GridTrack* track)
{
    auto type = (GridTrackSizeType)track->trackType();
    if (track->trackMaxType() != 0)
    {
        return YGGridTrackSize::minmax(
            gridSizeLength(type, track->trackValue()),
            gridSizeLength((GridTrackSizeType)(track->trackMaxType() - 1),
                           track->trackMaxValue()));
    }
    switch (type)
    {
        case GridTrackSizeType::points:
            return YGGridTrackSize::length(track->trackValue());
        case GridTrackSizeType::percent:
            return YGGridTrackSize::percent(track->trackValue());
        case GridTrackSizeType::fr:
            return YGGridTrackSize::fr(track->trackValue());
        default:
            return YGGridTrackSize::auto_();
    }
}

} // namespace

// The start line for a placement value expressed as a *cell* index.
//
// 0 is auto. Positive N already names the line that starts cell N. Negative
// values count back from the last cell, which needs a shift: the engine
// resolves line -1 to the grid's final line, and an item occupies the track
// *starting* at its line, so -1 alone would land in an implicit track past the
// end. Sending -2 for -1 puts the item in the last cell instead.
//
// Out-of-range negatives are left alone — the engine grows implicit tracks
// leftwards, the same as CSS.
static YGGridLine gridLine(int cell)
{
    if (cell == 0)
    {
        return YGGridLine::auto_();
    }
    return YGGridLine::fromInteger(cell < 0 ? cell - 1 : cell);
}

static YGGridLine gridSpan(uint32_t span)
{
    return span > 1 ? YGGridLine::span((int)span) : YGGridLine::auto_();
}

void GridTrack::syncContainerStyle(YGStyle& ygStyle,
                                   ContainerComponent* owner,
                                   uint32_t justifyItemsValue)
{
    facebook::yoga::GridTrackList lists[4];
    for (auto child : owner->children())
    {
        if (!child->is<GridTrack>())
        {
            continue;
        }
        auto track = child->as<GridTrack>();
        if (track->collection() > 3)
        {
            continue;
        }
        lists[track->collection()].push_back(gridTrackSize(track));
    }
    ygStyle.setGridTemplateColumns(std::move(lists[0]));
    ygStyle.setGridTemplateRows(std::move(lists[1]));
    ygStyle.setGridAutoColumns(std::move(lists[2]));
    ygStyle.setGridAutoRows(std::move(lists[3]));
    ygStyle.setJustifyItems((YGJustify)justifyItemsValue);
}

void GridTrack::syncStackContainerStyle(YGStyle& ygStyle,
                                        uint32_t justifyItemsValue)
{
    facebook::yoga::GridTrackList single;
    single.push_back(YGGridTrackSize::fr(1));
    facebook::yoga::GridTrackList singleRow;
    singleRow.push_back(YGGridTrackSize::fr(1));
    ygStyle.setGridTemplateColumns(std::move(single));
    ygStyle.setGridTemplateRows(std::move(singleRow));
    ygStyle.setGridAutoColumns(facebook::yoga::GridTrackList{});
    ygStyle.setGridAutoRows(facebook::yoga::GridTrackList{});
    ygStyle.setJustifyItems((YGJustify)justifyItemsValue);
}

// The inline-axis (width) self-alignment for a grid item, downgrading a
// resolved `stretch` to `flex-start` when the item hugs — a hug item sizes to
// content and cannot stretch. justify-self defaults to auto (inherit the
// container's justify-items, which itself defaults to stretch), so without this
// a hug item would fill its cell's inline axis.
static YGJustify resolveItemJustifySelf(uint32_t justifySelfValue,
                                        bool inlineHugs,
                                        uint32_t containerJustifyItems)
{
    if (!inlineHugs)
    {
        return (YGJustify)justifySelfValue;
    }
    uint32_t effective = justifySelfValue == (uint32_t)YGJustifyAuto
                             ? containerJustifyItems
                             : justifySelfValue;
    return effective == (uint32_t)YGJustifyStretch
               ? YGJustifyFlexStart
               : (YGJustify)justifySelfValue;
}

// A stack collapses every child into its single cell. There is no placement
// object on a stack child to write this, so the container's sync does it.
//
// No matching reset for the grid case: nothing can reparent an item or change a
// parent's layoutType at runtime (neither property animates or binds), so a
// style that was never written stays at yoga's auto-placement default. The
// editor, where both *can* happen, resets in its own sync.
void GridTrack::syncItemLines(YGStyle& ygStyle,
                              int32_t column,
                              int32_t row,
                              uint32_t columnSpan,
                              uint32_t rowSpan)
{
    ygStyle.setGridColumnStart(gridLine(column));
    ygStyle.setGridColumnEnd(gridSpan(columnSpan));
    ygStyle.setGridRowStart(gridLine(row));
    ygStyle.setGridRowEnd(gridSpan(rowSpan));
}

void GridTrack::syncStackItemCell(YGStyle& ygStyle)
{
    ygStyle.setGridColumnStart(YGGridLine::fromInteger(1));
    ygStyle.setGridColumnEnd(YGGridLine::auto_());
    ygStyle.setGridRowStart(YGGridLine::fromInteger(1));
    ygStyle.setGridRowEnd(YGGridLine::auto_());
}

// Inline-axis self alignment. Stays with the style rather than moving to the
// placement child: it resolves against the container's justify-items and the
// item's hug state, which syncStyle already has and a child would have to walk
// for.
void GridTrack::syncItemJustifySelf(YGStyle& ygStyle,
                                    uint32_t justifySelfValue,
                                    bool stack,
                                    bool inlineHugs,
                                    uint32_t containerJustifyItems)
{
    if (stack)
    {
        // A stack's container justify comes from its alignment (start/center/
        // end, never stretch), so the hug->flexStart downgrade that guards grid
        // stretch doesn't apply. Keep the item's own justify-self (auto
        // inherits the container's alignment) so horizontal alignment takes
        // effect.
        ygStyle.setJustifySelf((YGJustify)justifySelfValue);
        return;
    }
    ygStyle.setJustifySelf(resolveItemJustifySelf(justifySelfValue,
                                                  inlineHugs,
                                                  containerJustifyItems));
}
#endif
