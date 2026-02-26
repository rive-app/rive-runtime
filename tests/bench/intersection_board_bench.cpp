/*
 * Copyright 2024 Rive
 */

#include "bench.hpp"

#include "../src/intersection_board.hpp"
#include "common/intersection_board_reference_impl.hpp"

#include "marty_bboxes_187_copies.hpp"
#include "paper_bboxes_6_copies.hpp"

using namespace rive;
using namespace rive::gpu;

template <GroupingType Type> class IntersectionTileBenchBase : public Bench
{
public:
    void setup() override
    {
        // Prime the std::vectors.
        run();
    }

private:
    int run() const override
    {
        srand(0);
        m_tile.reset(0, 0, 0);
        uint16_t idx = 0;
        for (size_t i = 0; i < 10000; ++i)
        {
            int4 box = int4{rand(), rand(), rand(), rand()} & 0xff;
            int4 ltrb;
            ltrb.xy = simd::min(box.xy, box.zw);
            ltrb.zw = simd::max(box.xy, box.zw);
            ltrb = simd::min(ltrb, int4(254));
            ltrb.zw += 1;

            auto r = m_tile.findMaxIntersectingGroupIndex<Type>(ltrb, {});
            uint16_t idx = simd::reduce_max(r.maxGroupIndices) + 1;
            m_tile.addRectangle<Type>(ltrb, idx, 0);
        }
        return idx;
    }

#if 1
    mutable IntersectionTile m_tile;
#else
    mutable IntersectionBoardReferenceImpl m_tile;
#endif
};

class IntersectionTileBench
    : public IntersectionTileBenchBase<GroupingType::disjoint>
{};
REGISTER_BENCH(IntersectionTileBench);

class IntersectionTileBenchWithOverlap
    : public IntersectionTileBenchBase<GroupingType::overlapAllowed>
{};

REGISTER_BENCH(IntersectionTileBenchWithOverlap);

class IntersectionBoardBench : public Bench
{
public:
    IntersectionBoardBench(uint32_t width,
                           uint32_t height,
                           const int4* bboxes,
                           size_t bboxCount) :
        m_width(width),
        m_height(height),
        m_bboxes(bboxes),
        m_bboxCount(bboxCount),
        m_board{GroupingType::disjoint}
    {}

    void setup() override
    {
        // Prime the std::vectors.
        run();
    }

private:
    int run() const override
    {
        uint16_t maxIdx = 0;
        m_board.resizeAndReset(m_width, m_height);
        for (size_t i = 0; i < m_bboxCount; ++i)
        {
            uint16_t idx = m_board.addRectangle(m_bboxes[i]);
            maxIdx = std::max(idx, maxIdx);
        }
        return maxIdx;
    }

    uint32_t m_width;
    uint32_t m_height;
    const int4* m_bboxes;
    size_t m_bboxCount;

#if 1
    mutable IntersectionBoard m_board;
#else
    mutable IntersectionBoardReferenceImpl m_board;
#endif
};

class IntersectionBoardBench_paper : public IntersectionBoardBench
{
public:
    IntersectionBoardBench_paper() :
        IntersectionBoardBench(paper_bboxes_6_copies_window_width,
                               paper_bboxes_6_copies_window_height,
                               paper_bboxes_6_copies,
                               paper_bboxes_6_copies_bbox_count)
    {}
};

REGISTER_BENCH(IntersectionBoardBench_paper);

class IntersectionBoardBench_marty : public IntersectionBoardBench
{
public:
    IntersectionBoardBench_marty() :
        IntersectionBoardBench(marty_bboxes_187_copies_window_width,
                               marty_bboxes_187_copies_window_height,
                               marty_bboxes_187_copies,
                               marty_bboxes_187_copies_bbox_count)
    {}
};

REGISTER_BENCH(IntersectionBoardBench_marty);
