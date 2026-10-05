#include "rive/layout/layout_enums.hpp"
#include <catch.hpp>

// containerAlignment resolves the authored selector onto a container's own
// axes for both the yoga sync and the scroll virtualizer. Mirrors
// layout_container_alignment_test.dart in rive_core.

using namespace rive;

TEST_CASE("containerAlignment resolves onto row and column axes",
          "[layout][virtual_layout]")
{
    using M = LayoutMainDistribute;
    using X = LayoutCrossAlign;
    struct Row
    {
        LayoutAlignmentType type;
        M rowMain;
        X rowCross;
        M columnMain;
        X columnCross;
    };
    const Row rows[] = {
        {LayoutAlignmentType::topLeft, M::start, X::start, M::start, X::start},
        {LayoutAlignmentType::topCenter,
         M::center,
         X::start,
         M::start,
         X::center},
        {LayoutAlignmentType::topRight, M::end, X::start, M::start, X::end},
        {LayoutAlignmentType::centerLeft,
         M::start,
         X::center,
         M::center,
         X::start},
        {LayoutAlignmentType::center,
         M::center,
         X::center,
         M::center,
         X::center},
        {LayoutAlignmentType::centerRight,
         M::end,
         X::center,
         M::center,
         X::end},
        {LayoutAlignmentType::bottomLeft, M::start, X::end, M::end, X::start},
        {LayoutAlignmentType::bottomCenter,
         M::center,
         X::end,
         M::end,
         X::center},
        {LayoutAlignmentType::bottomRight, M::end, X::end, M::end, X::end},
        // Space-between always distributes the main axis.
        {LayoutAlignmentType::spaceBetweenStart,
         M::spaceBetween,
         X::start,
         M::spaceBetween,
         X::start},
        {LayoutAlignmentType::spaceBetweenCenter,
         M::spaceBetween,
         X::center,
         M::spaceBetween,
         X::center},
        {LayoutAlignmentType::spaceBetweenEnd,
         M::spaceBetween,
         X::end,
         M::spaceBetween,
         X::end},
    };
    for (const auto& row : rows)
    {
        INFO("type " << (int)row.type);
        auto asRow = containerAlignment(row.type, true);
        CHECK(asRow.main == row.rowMain);
        CHECK(asRow.cross == row.rowCross);
        auto asColumn = containerAlignment(row.type, false);
        CHECK(asColumn.main == row.columnMain);
        CHECK(asColumn.cross == row.columnCross);
    }
}
