/*
 * Copyright 2024 Rive
 */

#include "../src/intersection_board.hpp"
#include "common/intersection_board_reference_impl.hpp"
#include <catch.hpp>

namespace rive::gpu
{
template <typename T>
uint16_t find_standalone_max_group_index(T& impl, int4 ltrb)
{
    return simd::reduce_max(impl.findMaxIntersectingGroupIndex(ltrb, 0));
}

template <typename T>
void check_simple_intersections(T& impl, int top, int left)
{
    impl.reset(top, left, 0);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 0);
    impl.addRectangle(int4{-1, -1, 10, 10}, 1);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 1);

    impl.reset(top, left, 3);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 3);
    impl.addRectangle(int4{-1, -1, 10, 10}, 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    impl.addRectangle(int4{10, 10, 100, 100}, 5);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 11, 10}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 11}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 11, 10}) == 4);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 11, 11}) == 5);
    impl.addRectangle(int4{9, 10, 100, 100}, 6);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    impl.addRectangle(int4{10, 9, 100, 100}, 7);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 4);
    impl.addRectangle(int4{9, 9, 100, 100}, 8);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 8);
}

template <typename T> void check_maximal_rectangles(T& impl)
{
    impl.reset(0, 0, 0);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 0);
    impl.addRectangle(int4{-1, -1, 10, 10}, 3);
    CHECK(find_standalone_max_group_index(impl, int4{-1, -1, 10, 10}) == 3);

    CHECK(find_standalone_max_group_index(impl, int4{0, 0, 255, 255}) == 3);
    impl.addRectangle(int4{0, 0, 255, 255}, 7);
    CHECK(find_standalone_max_group_index(impl, int4{1, 1, 2, 2}) == 7);
    CHECK(find_standalone_max_group_index(impl, int4{0, 0, 255, 255}) == 7);

    CHECK(find_standalone_max_group_index(impl, int4{-999, -999, 999, 999}) ==
          7);
    impl.addRectangle(int4{-999, -999, 999, 999}, 6000);
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
    impl.addRectangle(int4{59, 131, 205, 181}, 32765);
    CHECK(find_standalone_max_group_index(impl, int4{79, -90, 182, 324}) ==
          32765);
    impl.addRectangle(int4{79, -90, 182, 324}, 32766);
    CHECK(find_standalone_max_group_index(impl, int4{145, 93, 166, 214}) ==
          32766);
    impl.addRectangle(int4{145, 93, 166, 214}, 32767);
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
        int l = tileLeft + rand_range(-width + 1, 254);
        int t = tileTop + rand_range(-height + 1, 254);
        int4 ltrb = {l, t, l + width, t + height};
        uint16_t idx = find_standalone_max_group_index(ref, ltrb);
        uint16_t fastIdx = find_standalone_max_group_index(fast, ltrb);
        CHECK(fastIdx == idx);
        ref.addRectangle(ltrb, idx + 1);
        fast.addRectangle(ltrb, fastIdx + 1);
    }
}

TEST_CASE("IntersectionTile", "IntersectionBoard")
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
    IntersectionBoard fast;

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
        for (int16_t i = 1; i < layerCount; ++i)
        {
            ref.addRectangle(ltrb);
        }
        CHECK(refIdx == fast.addRectangle(ltrb, layerCount));
    }
}

void check_intersection_board_random_rectangles2(size_t n)
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast;

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

TEST_CASE("IntersectionBoard", "IntersectionBoard")
{
    IntersectionBoardReferenceImpl ref;
    IntersectionBoard fast;

    check_intersection_board_corner_cases(ref);
    check_intersection_board_corner_cases(fast);

    // Offscreen and empty rectangles get discarded
    fast.resizeAndReset(1000, 1000);
    CHECK(fast.addRectangle({0, 0, 0, 1}) == 0);
    CHECK(fast.addRectangle({0, 0, 1, 0}) == 0);
    CHECK(fast.addRectangle({1000, 999, 1001, 1001}) == 0);
    CHECK(fast.addRectangle({999, 1000, 1001, 1001}) == 0);
    CHECK(fast.addRectangle({8, 8, 8, 9}) == 0);
    CHECK(fast.addRectangle({8, 8, 9, 8}) == 0);

    srand(0);
    check_intersection_board_random_rectangles(100, 1000);
    check_intersection_board_random_rectangles(500, 1000);
    check_intersection_board_random_rectangles(1000, 1000);
    check_intersection_board_random_rectangles(10000, 1000);
    check_intersection_board_random_rectangles2(1000);
}

// Check inserting rectangles that have more than one layer.
TEST_CASE("layerCount", "IntersectionBoard")
{
    IntersectionBoard board;
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
} // namespace rive::gpu
