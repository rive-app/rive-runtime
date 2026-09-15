#ifndef _RIVE_GRID_TRACK_HPP_
#define _RIVE_GRID_TRACK_HPP_
#include "rive/generated/layout/grid_track_base.hpp"
#ifdef WITH_RIVE_LAYOUT
class YGStyle;
#endif
namespace rive
{
class ContainerComponent;

// Track list a GridTrack belongs to; matches the editor enum order.
enum class GridTrackCollection : uint8_t
{
    templateColumns,
    templateRows,
    autoColumns,
    autoRows
};

// Sizing function of a track; matches the editor enum order. For the optional
// max (trackMaxType), 0 means none and the remaining values are offset by one.
enum class GridTrackSizeType : uint8_t
{
    autoSize,
    points,
    percent,
    fr
};

class GridTrack : public GridTrackBase
{
public:
    GridTrackCollection gridCollection() const
    {
        return (GridTrackCollection)collection();
    }

    void collectionChanged() override;
    void trackTypeChanged() override;
    void trackValueChanged() override;
    void trackMaxTypeChanged() override;
    void trackMaxValueChanged() override;

#ifdef WITH_RIVE_LAYOUT
    // Shared grid style mapping used by LayoutComponent::syncStyle and
    // LayoutParticipant::syncStyleChanges.
    /// Writes an explicit placement's four grid lines. Takes the raw cell and
    /// span values rather than exposing YGGridLine: that name is a yoga type
    /// alias, so it can't be forward declared, and this header reaches
    /// core_registry.hpp — pulling yoga in here would put it everywhere.
    static void syncItemLines(YGStyle& ygStyle,
                              int32_t column,
                              int32_t row,
                              uint32_t columnSpan,
                              uint32_t rowSpan);

    static void syncContainerStyle(YGStyle& ygStyle,
                                   ContainerComponent* owner,
                                   uint32_t justifyItemsValue);
    // Stack container: a synthetic 1x1 grid (1fr x 1fr) whose single cell fills
    // the container; children all land in cell 1,1 (see syncStackItemCell
    // stack).
    static void syncStackContainerStyle(YGStyle& ygStyle,
                                        uint32_t justifyItemsValue);
    /// Collapses a stack child into the single cell. Grid children need no
    /// counterpart: an explicit GridItemPlacement applies itself, and an
    /// auto-placed one is already at yoga's default.
    static void syncStackItemCell(YGStyle& ygStyle);
    /// Inline-axis self alignment, resolved against container context.
    static void syncItemJustifySelf(YGStyle& ygStyle,
                                    uint32_t justifySelfValue,
                                    bool stack,
                                    bool inlineHugs,
                                    uint32_t containerJustifyItems);
#endif

private:
    void markLayoutDirty();
};
} // namespace rive

#endif
