/*
 * Copyright 2024 Rive
 */

#include "../src/intersection_board.hpp"
#include "common/intersection_board_reference_impl.hpp"
#include <catch.hpp>
#include <tuple>

namespace rive::gpu
{

template <GroupingType Type, typename T>
std::tuple<int16_t, int16_t>
find_standalone_max_group_index_and_overlappability(T& impl, int4 ltrb)
{
    auto r = impl.template findMaxIntersectingGroupIndex<Type>(ltrb, {});

    auto maxIndex = simd::reduce_max(r.maxGroupIndices);
    int16_t fullOverlap = 0;

    // The individual tiles allow overlappability to be written (as an inner
    // loop optimization) when no overlap has occurred, so filter that out for
    // testing (the intersection board also handles this)
    if (maxIndex != 0)
    {
        for (int i = 0; i < 8; i++)
        {
            if (r.maxGroupIndices[i] == maxIndex)
            {
                fullOverlap |= r.overlapBits[i];
            }
        }
    }

    return {maxIndex, fullOverlap};
}

template <GroupingType Type = GroupingType::disjoint, typename T>
int16_t find_standalone_max_group_index(T& impl, int4 ltrb)
{
    return std::get<0>(
        find_standalone_max_group_index_and_overlappability<Type>(impl, ltrb));
}

template <GroupingType Type, typename T>
uint16_t find_standalone_overlappability(T& impl, int4 ltrb)
{
    return std::get<1>(
        find_standalone_max_group_index_and_overlappability<Type>(impl, ltrb));
}

void add_tile_rectangle(IntersectionBoardReferenceImpl& ref,
                        int4 ltrb,
                        int16_t groupIndex)
{
    ref.addRectangle(ltrb, groupIndex);
}

void add_tile_rectangle(IntersectionTile& tile, int4 ltrb, int16_t groupIndex)
{
    tile.testingOnly_addRectangleAndValidate<GroupingType::disjoint>(ltrb,
                                                                     groupIndex,
                                                                     0);
}

template <typename T>
void check_simple_intersections(T& impl, int top, int left)
{
    impl.reset(top, left, 0);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 0);
    add_tile_rectangle(impl, int4{-1, -1, 10, 10}, 1);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 1);

    impl.reset(top, left, 3);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 3);
    add_tile_rectangle(impl, int4{-1, -1, 10, 10}, 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    add_tile_rectangle(impl, int4{10, 10, 100, 100}, 5);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 11, 10}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 11}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 11, 10}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 11, 11}) == 5);
    add_tile_rectangle(impl, int4{9, 10, 100, 100}, 6);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    add_tile_rectangle(impl, int4{10, 9, 100, 100}, 7);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    add_tile_rectangle(impl, int4{9, 9, 100, 100}, 8);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 8);
}

template <typename T> void check_maximal_rectangles(T& impl)
{
    impl.reset(0, 0, 0);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 0);
    add_tile_rectangle(impl, int4{-1, -1, 10, 10}, 3);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 3);

    CHECK(find_standalone_max_group_index(impl,
                                          int4{0,
                                               0,
                                               IntersectionTile::TILE_DIM,
                                               IntersectionTile::TILE_DIM}) ==
          3);
    add_tile_rectangle(
        impl,
        int4{0, 0, IntersectionTile::TILE_DIM, IntersectionTile::TILE_DIM},
        7);
    CHECK(find_standalone_max_group_index(impl, int4{1, 1, 2, 2}) == 7);
    CHECK(find_standalone_max_group_index(impl,
                                          int4{0,
                                               0,
                                               IntersectionTile::TILE_DIM,
                                               IntersectionTile::TILE_DIM}) ==
          7);

    CHECK(find_standalone_max_group_index(impl, int4{-999, -999, 999, 999}) ==
          7);
    add_tile_rectangle(impl, int4{-999, -999, 999, 999}, 6000);
    CHECK(find_standalone_max_group_index(impl, int4{1, 1, 2, 2}) == 6000);
    CHECK(find_standalone_max_group_index(impl, int4{-999, -999, 999, 999}) ==
          6000);
}

template <typename T> void check_maximal_group_ids(T& impl)
{
    impl.reset(0, 0, 32767);
    CHECK(find_standalone_max_group_index(impl, int4{59, 131, 205, 181}) ==
          32767);

    impl.reset(0, 0, 32764);
    CHECK(find_standalone_max_group_index(impl, int4{59, 131, 205, 181}) ==
          32764);
    add_tile_rectangle(impl, int4{59, 131, 205, 181}, 32765);
    CHECK(find_standalone_max_group_index(impl, int4{79, -90, 182, 324}) ==
          32765);
    add_tile_rectangle(impl, int4{79, -90, 182, 324}, 32766);
    CHECK(find_standalone_max_group_index(impl, int4{145, 93, 166, 214}) ==
          32766);
    add_tile_rectangle(impl, int4{145, 93, 166, 214}, 32767);
    CHECK(find_standalone_max_group_index(impl, int4{12, -133, 104, 1}) ==
          32766);
    CHECK(find_standalone_max_group_index(impl, int4{12, -133, 104, 328}) ==
          32766);
    CHECK(find_standalone_max_group_index(impl, int4{12, -133, 150, 328}) ==
          32767);
}

static int rand_range(int min, int max)
{
    int mag = max - min + 1;
    return (rand() % mag) + min;
}

void check_multiple_rectangles(IntersectionBoardReferenceImpl& ref,
                               IntersectionTile& fast,
                               int maxSize,
                               size_t n)
{
    int tileLeft = rand() % 6000 - 3000;
    int tileTop = rand() % 6000 - 3000;
    uint16_t baseline = rand() % 10;
    ref.reset(tileLeft, tileTop, baseline);
    fast.reset(tileLeft, tileTop, baseline);

    for (size_t i = 0; i < n; ++i)
    {
        int width = (rand() % maxSize) + 1;
        int height = (rand() % maxSize) + 1;
        int l =
            tileLeft + rand_range(-width + 1, IntersectionTile::TILE_DIM - 1);
        int t =
            tileTop + rand_range(-height + 1, IntersectionTile::TILE_DIM - 1);
        int4 ltrb = {l, t, l + width, t + height};
        uint16_t idx = find_standalone_max_group_index(ref, ltrb);
        uint16_t fastIdx = find_standalone_max_group_index(fast, ltrb);
        CHECK(fastIdx == idx);
        ref.addRectangle(ltrb, idx + 1);
        fast.testingOnly_addRectangleAndValidate<GroupingType::disjoint>(
            ltrb,
            fastIdx + 1,
            0);
    }
}

TEST_CASE("IntersectionTile", "[IntersectionBoard]")
{
    IntersectionBoardReferenceImpl ref;
    IntersectionTile fast;
    check_simple_intersections(ref, 0, 0);
    check_simple_intersections(fast, 0, 0);
    check_simple_intersections(ref, -100, -65);
    check_simple_intersections(fast, -100, -65);
    check_simple_intersections(ref, 8, 7);
    check_simple_intersections(fast, 8, 7);

    check_maximal_rectangles(ref);
    check_maximal_rectangles(fast);

    check_maximal_group_ids(ref);
    check_maximal_group_ids(fast);

    srand(0);
    check_multiple_rectangles(ref, fast, 32, 1000);
    check_multiple_rectangles(ref, fast, 1, 1000);
    check_multiple_rectangles(ref, fast, 65, 1000);
    check_multiple_rectangles(ref, fast, 200, 1000);
    check_multiple_rectangles(ref, fast, 255, 1000);
    check_multiple_rectangles(ref,
                              fast,
                              500,
                              1000); // Exercise maximal rectangles.
    check_multiple_rectangles(ref,
                              fast,
                              1000,
                              1000); // Exercise maximal rectangles.
}

template <typename T> void check_intersection_board_corner_cases(T& impl)
{
    impl.resizeAndReset(1000, 1000);
    CHECK(impl.addRectangle(int4{1, 1, 800, 600}) == 1);
    CHECK(impl.addRectangle(int4{799, 599, 999, 999}) == 2);
    const int32_t kMax32i = std::numeric_limits<int32_t>::max();
    const int32_t kMin32i = std::numeric_limits<int32_t>::min();
    CHECK(impl.addRectangle(int4{kMin32i, kMin32i, kMax32i, kMax32i}) == 3);

    // Check tile boundaries.
    impl.resizeAndReset(800, 600);
    CHECK(impl.addRectangle(int4{254, 254, 256, 256}) == 1);
    CHECK(impl.addRectangle(int4{254, 254, 255, 255}) == 2);
    CHECK(impl.addRectangle(int4{255, 0, 510, 255}) == 2);
    CHECK(impl.addRectangle(int4{255, 255, 256, 510}) == 2);
    CHECK(impl.addRectangle(int4{0, 255, 255, 256}) == 2);
    CHECK(impl.addRectangle(int4{0, 0, 800, 600}) == 3);

    // Check tiles that stretch offscreen.
    impl.resizeAndReset(1600, 1200);
    CHECK(impl.addRectangle(int4{-10000, 500, 10000, 501}) == 1);
    CHECK(impl.addRectangle(int4{8, -10000, 9, 10000}) == 2);
    CHECK(impl.addRectangle(int4{-10000, 0, 10000, 1}) == 3);
    CHECK(impl.addRectangle(
              int4{-999999999, -999999999, 999999999, 999999999}) == 4);
    CHECK(impl.addRectangle(int4{1, 2, 3, 4}) == 5);
}

void check_intersection_board_random_rectangles(int maxSize, size_t n)
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast{GroupingType::disjoint};

    ref.resizeAndReset(3840, 2160);
    fast.resizeAndReset(3840, 2160);

    for (size_t i = 0; i < n; ++i)
    {
        int width = (rand() % maxSize) + 1;
        int height = (rand() % maxSize) + 1;
        int l = rand_range(-width + 1, 3840 - 1);
        int t = rand_range(-height + 1, 2160 - 1);
        int4 ltrb = {l, t, l + width, t + height};
        int16_t layerCount = rand() % 5 + 1;
        int16_t refIdx = ref.addRectangle(ltrb);
        for (int16_t j = 1; j < layerCount; ++j)
        {
            ref.addRectangle(ltrb);
        }
        CHECK(refIdx == fast.addRectangle(ltrb, layerCount));
    }
}

void check_intersection_board_random_rectangles2(size_t n)
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast{GroupingType::disjoint};

    ref.resizeAndReset(3840, 2160);
    fast.resizeAndReset(3840, 2160);

    for (size_t i = 0; i < n; ++i)
    {
        int4 box =
            int4{rand(), rand(), rand(), rand()} % int4{3840, 2160, 3840, 2160};
        int4 ltrb;
        ltrb.xy = simd::min(box.xy, box.zw);
        ltrb.zw = simd::max(box.xy, box.zw);
        ltrb.zw += 1;
        CHECK(ref.addRectangle(ltrb) == fast.addRectangle(ltrb));
    }
}

TEST_CASE("IntersectionBoard", "[IntersectionBoard]")
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast{GroupingType::disjoint};

    check_intersection_board_corner_cases(ref);
    check_intersection_board_corner_cases(fast);

    // Offscreen and empty rectangles get discarded
    fast.resizeAndReset(1000, 1000);
    CHECK(fast.addRectangle(int4{0, 0, 0, 1}) == 0);
    CHECK(fast.addRectangle(int4{0, 0, 1, 0}) == 0);
    CHECK(fast.addRectangle(int4{1000, 999, 1001, 1001}) == 0);
    CHECK(fast.addRectangle(int4{999, 1000, 1001, 1001}) == 0);
    CHECK(fast.addRectangle(int4{8, 8, 8, 9}) == 0);
    CHECK(fast.addRectangle(int4{8, 8, 9, 8}) == 0);

    srand(0);
    check_intersection_board_random_rectangles(100, 1000);
    check_intersection_board_random_rectangles(500, 1000);
    check_intersection_board_random_rectangles(1000, 1000);
    check_intersection_board_random_rectangles(10000, 1000);
    check_intersection_board_random_rectangles2(1000);
}

// Check inserting rectangles that have more than one layer.
TEST_CASE("layerCount", "[IntersectionBoard]")
{
    IntersectionBoard board{GroupingType::disjoint};
    board.resizeAndReset(800, 600);
    CHECK(board.addRectangle(int4{254, 254, 256, 256}, 7) == 1);

    // Since the previous rect had 7 layers, the next group index is 8.
    CHECK(board.addRectangle(int4{254, 254, 255, 255}, 1) == 8);
    CHECK(board.addRectangle(int4{255, 0, 510, 255}, 2) == 8);
    CHECK(board.addRectangle(int4{255, 255, 256, 510}, 3) == 8);
    CHECK(board.addRectangle(int4{0, 255, 255, 256}, 4) == 8);

    CHECK(board.addRectangle(int4{0, 254, 800, 255}) == 10);
    CHECK(board.addRectangle(int4{0, 255, 800, 256}) == 12);
    CHECK(board.addRectangle(int4{0, 0, 800, 600}) == 13);
}

TEST_CASE("BasicTileOverlap", "[IntersectionBoard]")
{
    IntersectionTile tile;

    tile.reset(0, 0, 0);

    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{0, 0, 20, 20},
        1,
        0x0001);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{40, 40, 60, 60},
        1,
        0x0002);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{60, 60, 80, 80},
        1,
        0x0004);

    // Add a bunch of rectangles to push the higher group element into the next
    //  chunk
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        1,
        0x7fff);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        1,
        0x7fff);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        1,
        0x7fff);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        1,
        0x7fff);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        1,
        0x7fff);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        1,
        0x7fff);

    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{80, 80, 100, 100},
        2,
        0x0008);

    // Ensure that we get no overlappability bits if we didn't overlap any
    // rectangles

    auto [index, overlap] = find_standalone_max_group_index_and_overlappability<
        GroupingType::overlapAllowed>(tile, int4{100, 100, 120, 120});
    CHECK(index == 0);
    CHECK(overlap == 0x0000);

    std::tie(index, overlap) =
        find_standalone_max_group_index_and_overlappability<
            GroupingType::overlapAllowed>(tile, int4{0, 0, 40, 40});
    CHECK(index == 1);
    CHECK(overlap == 0x0001);

    std::tie(index, overlap) =
        find_standalone_max_group_index_and_overlappability<
            GroupingType::overlapAllowed>(tile, int4{0, 0, 41, 41});
    CHECK(index == 1);
    CHECK(overlap == 0x0003);

    std::tie(index, overlap) =
        find_standalone_max_group_index_and_overlappability<
            GroupingType::overlapAllowed>(tile, int4{0, 0, 61, 61});
    CHECK(index == 1);
    CHECK(overlap == 0x0007);

    std::tie(index, overlap) =
        find_standalone_max_group_index_and_overlappability<
            GroupingType::overlapAllowed>(tile, int4{0, 0, 81, 81});
    CHECK(index == 2);
    CHECK(overlap == 0x0008);
}

void check_intersection_board_random_rectangles_overlappability(int maxSize,
                                                                size_t n)
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast{GroupingType::overlapAllowed};

    ref.resizeAndReset(3840, 2160);
    fast.resizeAndReset(3840, 2160);

    for (size_t i = 0; i < n; ++i)
    {
        int width = (rand() % maxSize) + 1;
        int height = (rand() % maxSize) + 1;
        int l = rand_range(-width + 1, 3840 - 1);
        int t = rand_range(-height + 1, 2160 - 1);
        int4 ltrb = {l, t, l + width, t + height};

        auto destOverlap =
            uint16_t((1 << (rand() % 16)) | (1 << (rand() % 16)));
        auto overlappabilityMask =
            uint16_t((1 << (rand() % 16)) | (1 << (rand() % 16)));

        int16_t refIdx = ref.addRectangle(ltrb,
                                          GroupingType::overlapAllowed,
                                          destOverlap,
                                          overlappabilityMask);
        int16_t testIdx =
            fast.addRectangle(ltrb, destOverlap, overlappabilityMask, 1);

        CHECK(refIdx == testIdx);
    }
}

TEST_CASE("Basic IntersectionBoard overlappability", "[IntersectionBoard]")
{
    IntersectionBoard board{GroupingType::overlapAllowed};
    board.resizeAndReset(100, 100);

    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 1, 0, 1) == 1);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 2, 0, 1) == 1);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 4, 0, 1) == 1);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 8, 0, 1) == 1);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 16, 1, 1) == 2);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 8, 1, 1) == 2);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 4, 1, 1) == 2);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 2, 1, 1) == 2);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 1, 1, 1) == 2);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 8, 1, 1) == 3);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 8, 1, 1) == 3);
    CHECK(board.addRectangle(int4{0, 0, 100, 100}, 8, 9, 1) == 4);
}

TEST_CASE("IntersectionBoard overlap baseline", "[IntersectionBoard]")
{
    // Full-sized tile, greater max group (no rectangles remaining)

    IntersectionTile tile;
    tile.reset(0, 0, 0, 0);

    constexpr auto TILE_DIM = IntersectionTile::TILE_DIM;

    auto fullTile = int4{0, 0, TILE_DIM, TILE_DIM};

    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        fullTile,
        1,
        0x1001);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    CHECK(tile.testingOnly_baselineGroupIndex() == 1);
    CHECK(tile.testingOnly_maxGroupIndex() == 1);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x1001);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0x1001);

    CHECK(find_standalone_max_group_index<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 1);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 0x1001);

    // Adding a new rectangle to the baseline group that has only bits in the
    // baseline group should do nothing.
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{0, 0, 20, 20},
        1,
        0x0000);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{0, 0, 20, 20},
        1,
        0x0001);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{0, 0, 20, 20},
        1,
        0x1000);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{0, 0, 20, 20},
        1,
        0x1001);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    CHECK(find_standalone_max_group_index<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 1);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 0x1001);

    CHECK(tile.testingOnly_baselineGroupIndex() == 1);
    CHECK(tile.testingOnly_maxGroupIndex() == 1);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x1001);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0x1001);

    // New rectangles in the baseline should be added correctly, though.
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{0, 0, 20, 20},
        1,
        0x0010);
    CHECK(tile.testingOnly_rectangleCount() == 1);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{20, 20, 40, 40},
        1,
        0x1040);
    CHECK(tile.testingOnly_rectangleCount() == 2);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{40, 40, 60, 60},
        1,
        0x0021);
    CHECK(tile.testingOnly_rectangleCount() == 3);
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{60, 60, 80, 80},
        1,
        0x2181);
    CHECK(tile.testingOnly_rectangleCount() == 4);
    CHECK(tile.testingOnly_baselineGroupIndex() == 1);
    CHECK(tile.testingOnly_maxGroupIndex() == 1);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x1001);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0x31F1);

    // Test that detection in various spots gives the correct overlappability
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 0x1051);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{50, 50, 70, 70}) == 0x31A1);

    // Adding a new baseline rectangle that touches *none* of the existing bits
    // should leave all of the existing rectangles in that group.
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        fullTile,
        1,
        0x8002);
    CHECK(tile.testingOnly_rectangleCount() == 4);
    CHECK(tile.testingOnly_baselineGroupIndex() == 1);
    CHECK(tile.testingOnly_maxGroupIndex() == 1);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x9003);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0xB1F3);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 0x9053);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{50, 50, 70, 70}) == 0xB1A3);

    // Simiarly, test adding a new rectangle the doesn't change the baseline in
    // any way.
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        fullTile,
        1,
        0x9003);
    CHECK(tile.testingOnly_rectangleCount() == 4);
    CHECK(tile.testingOnly_baselineGroupIndex() == 1);
    CHECK(tile.testingOnly_maxGroupIndex() == 1);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x9003);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0xB1F3);

    // Adding a new baseline rectangle that touches *some* of the existing bits
    // should remove those rectangles, even if it doesn't add all the new bits
    // directly
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        fullTile,
        1,
        0x0030);

    // two rectangles (first and third) are now "under" these bits and should
    // have been removed
    CHECK(tile.testingOnly_rectangleCount() == 2);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x9033);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0xB1F3);

    // Adding a new baseline rectangle that causes the baseline to cover every
    // bit of them should result in 0 rectangles.
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        fullTile,
        1,
        0x21C0);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    CHECK(tile.testingOnly_baselineGroupIndex() == 1);
    CHECK(tile.testingOnly_maxGroupIndex() == 1);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0xB1F3);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0xB1F3);

    // A new full-tile rectangle in the next group up should replace the
    // baseline and max group overlappability bits.
    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        fullTile,
        2,
        0x000C);
    CHECK(tile.testingOnly_rectangleCount() == 0);
    CHECK(tile.testingOnly_baselineGroupIndex() == 2);
    CHECK(tile.testingOnly_maxGroupIndex() == 2);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x000C);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0x000C);

    tile.testingOnly_addRectangleAndValidate<GroupingType::overlapAllowed>(
        int4{70, 70, 90, 90},
        4,
        0x000F);
    CHECK(tile.testingOnly_baselineGroupIndex() == 2);
    CHECK(tile.testingOnly_maxGroupIndex() == 4);
    CHECK(tile.testingOnly_baselineOverlapBits() == 0x000C);
    CHECK(tile.testingOnly_overlapBitsForMaxGroup() == 0x000F);

    CHECK(find_standalone_max_group_index<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 2);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{10, 10, 30, 30}) == 0x000C);
    CHECK(find_standalone_max_group_index<GroupingType::overlapAllowed>(
              tile,
              int4{50, 50, 70, 70}) == 2);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{50, 50, 70, 70}) == 0x000C);
    CHECK(find_standalone_max_group_index<GroupingType::overlapAllowed>(
              tile,
              int4{60, 60, 80, 80}) == 4);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{60, 60, 80, 80}) == 0x000F);
    CHECK(find_standalone_max_group_index<GroupingType::overlapAllowed>(
              tile,
              int4{80, 80, 100, 100}) == 4);
    CHECK(find_standalone_overlappability<GroupingType::overlapAllowed>(
              tile,
              int4{80, 80, 100, 100}) == 0x000F);
}

TEST_CASE("IntersectionBoard overlappability", "[IntersectionBoard]")
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast{GroupingType::overlapAllowed};

    srand(0);
    check_intersection_board_random_rectangles_overlappability(100, 1000);
    check_intersection_board_random_rectangles_overlappability(500, 1000);
    check_intersection_board_random_rectangles_overlappability(1000, 1000);
    check_intersection_board_random_rectangles_overlappability(10000, 1000);
}

} // namespace rive::gpu
