/*
 * Copyright 2022 Rive
 */

#include "rive/core/type_conversions.hpp"
#include "rive/math/raw_path_utils.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/wangs_formula.hpp"
#include <cmath>

using namespace rive;

enum SegmentType
{ // must fit in 2 bits
    kLine,
    kQuad,
    kCubic,
    // unused for now
};

/*
 *  Inspired by Skia's SkContourMeasure
 */

ContourMeasure::ContourMeasure(std::vector<Segment>&& segs,
                               std::vector<Vec2D>&& pts,
                               float length,
                               bool isClosed) :
    m_segments(std::move(segs)), m_points(std::move(pts)), m_length(length), m_isClosed(isClosed)
{}

// Return index of the segment that contains distance,
// or the last segment if distance == m_distance
size_t ContourMeasure::findSegment(float distance) const
{
    assert(m_segments.front().m_distance > 0);
    assert(m_segments.back().m_distance == m_length);

    assert(distance >= 0 && distance <= m_length);

    const Segment seg = {distance, 0, 0, 0};
    auto iter = std::lower_bound(m_segments.begin(), m_segments.end(), seg);
    assert(iter != m_segments.end());
    assert(iter->m_distance >= distance);
    assert(iter->m_ptIndex < m_points.size());
    return iter - m_segments.begin();
}

static ContourMeasure::PosTan eval_quad(const Vec2D pts[], float t)
{
    assert(t >= 0 && t <= 1);

    const EvalQuad eval(pts);

    // Compute derivative as at + b
    auto a = two(eval.a);
    auto b = eval.b;

    return {
        eval(t),
        (a * t + b).normalized(),
    };
}

static ContourMeasure::PosTan eval_cubic(const Vec2D pts[], float t)
{
    assert(t >= 0 && t <= 1);

    const EvalCubic eval(pts);

    // Compute derivative as at^2 + bt + c;
    auto a = eval.a * 3;
    auto b = two(eval.b);
    auto c = eval.c;

    return {
        eval(t),
        ((a * t + b) * t + c).normalized(),
    };
}

void ContourMeasure::Segment::extract(RawPath* dst, const Vec2D pts[]) const
{
    pts += m_ptIndex;
    switch (m_type)
    {
        case SegmentType::kLine:
            dst->line(pts[1]);
            break;
        case SegmentType::kQuad:
            dst->quad(pts[1], pts[2]);
            break;
        case SegmentType::kCubic:
            dst->cubic(pts[1], pts[2], pts[3]);
            break;
    }
}

void ContourMeasure::Segment::extract(RawPath* dst,
                                      float fromT,
                                      float toT,
                                      const Vec2D pts[],
                                      bool moveTo) const
{
    assert(fromT <= toT);
    pts += m_ptIndex;

    Vec2D tmp[4];
    switch (m_type)
    {
        case SegmentType::kLine:
            line_extract(pts, fromT, toT, tmp);
            if (moveTo)
            {
                dst->move(tmp[0]);
            }
            dst->line(tmp[1]);
            break;
        case SegmentType::kQuad:
            quad_extract(pts, fromT, toT, tmp);
            if (moveTo)
            {
                dst->move(tmp[0]);
            }
            dst->quad(tmp[1], tmp[2]);
            break;
        case SegmentType::kCubic:
            cubic_extract(pts, fromT, toT, tmp);
            if (moveTo)
            {
                dst->move(tmp[0]);
            }
            dst->cubic(tmp[1], tmp[2], tmp[3]);
            break;
    }
}

ContourMeasure::PosTan ContourMeasure::getPosTan(float distance) const
{
    // specal-case end of the contour
    if (distance >= m_length)
    {
        size_t N = m_points.size();
        assert(N > 1);
        return {m_points[N - 1], (m_points[N - 1] - m_points[N - 2]).normalized()};
    }

    if (distance < 0)
    {
        distance = 0;
    }

    size_t i = this->findSegment(distance);
    assert(i < m_segments.size());
    const auto seg = m_segments[i];
    const float currD = seg.m_distance;
    const float prevD = i > 0 ? m_segments[i - 1].m_distance : 0;

    assert(prevD < currD);
    assert(distance <= currD);
    assert(distance >= prevD);

    const float relD = (distance - prevD) / (currD - prevD);
    assert(relD >= 0 && relD <= 1);

    if (seg.m_type == SegmentType::kLine)
    {
        assert(seg.m_ptIndex + 1 < m_points.size());
        auto p0 = m_points[seg.m_ptIndex + 0];
        auto p1 = m_points[seg.m_ptIndex + 1];
        return {
            Vec2D::lerp(p0, p1, relD),
            (p1 - p0).normalized(),
        };
    }

    float prevT = 0;
    if (i > 0)
    {
        auto prev = m_segments[i - 1];
        if (prev.m_ptIndex == seg.m_ptIndex)
        {
            prevT = prev.getT();
        }
    }

    const float t = lerp(prevT, seg.getT(), relD);
    assert(t >= 0 && t <= 1);

    if (seg.m_type == SegmentType::kQuad)
    {
        assert(seg.m_ptIndex + 2 < m_points.size());
        return eval_quad(&m_points[seg.m_ptIndex], t);
    }
    else
    {
        assert(seg.m_type == SegmentType::kCubic);
        assert(seg.m_ptIndex + 3 < m_points.size());
        return eval_cubic(&m_points[seg.m_ptIndex], t);
    }
}

static const ContourMeasure::Segment* next_segment_beginning(const ContourMeasure::Segment* seg)
{
    auto startingPtIndex = seg->m_ptIndex;
    do
    {
        seg += 1;
    } while (seg->m_ptIndex == startingPtIndex);
    return seg;
}

// Compute the (interpolated) t for a distance within the index'th segment
static float compute_t(Span<const ContourMeasure::Segment> segs, size_t index, float distance)
{
    const auto seg = segs[index];
    assert(distance <= seg.m_distance);

    float prevDist = 0, prevT = 0;
    if (index > 0)
    {
        const auto prev = segs[index - 1];
        prevDist = prev.m_distance;
        if (prev.m_ptIndex == seg.m_ptIndex)
        {
            prevT = prev.getT();
        }
    }

    assert(prevDist < seg.m_distance);
    const auto ratio = (distance - prevDist) / (seg.m_distance - prevDist);
    return lerp(prevT, seg.getT(), ratio);
}

void ContourMeasure::getSegment(float startDist,
                                float endDist,
                                RawPath* dst,
                                bool startWithMove) const
{
    // sanitize the inputs
    startDist = std::max(0.f, startDist);
    endDist = std::min(m_length, endDist);
    if (startDist >= endDist)
    {
        return;
    }

    const auto startIndex = this->findSegment(startDist);
    const auto endIndex = this->findSegment(endDist);

    const auto start = m_segments[startIndex];
    const auto end = m_segments[endIndex];

    const auto startT = compute_t(m_segments, startIndex, startDist);
    const auto endT = compute_t(m_segments, endIndex, endDist);

    if (start.m_ptIndex == end.m_ptIndex)
    {
        start.extract(dst, startT, endT, m_points.data(), startWithMove);
    }
    else
    {
        start.extract(dst, startT, 1, m_points.data(), startWithMove);

        // now scoop up all the segments after start, and before end
        const auto* seg = next_segment_beginning(&m_segments[startIndex]);
        while (seg->m_ptIndex != end.m_ptIndex)
        {
            seg->extract(dst, m_points.data());
            seg = next_segment_beginning(seg);
        }
        assert(seg->m_ptIndex == end.m_ptIndex);

        end.extract(dst, 0, endT, m_points.data(), false);
    }
}

void ContourMeasure::dump() const
{
    printf("length %g pts %zu segs %zu\n", m_length, m_points.size(), m_segments.size());
    for (const auto& s : m_segments)
    {
        printf(" %g %d %g %d\n", s.m_distance, s.m_ptIndex, s.getT(), s.m_type);
    }
}

//////////////////////////////////////////////////////////////////////////////

constexpr auto kMaxDot30 = ContourMeasure::kMaxDot30;

static inline unsigned toDot30(float x)
{
    assert(x >= 0 && x < 1);
    return (unsigned)(x * (1 << 30));
}

static void addSeg(std::vector<ContourMeasure::Segment>& array,
                   const ContourMeasure::Segment& seg,
                   bool required = false)
{
    if (array.size() > 0)
    {
        const auto& last = array.back();
        assert(last.m_distance <= seg.m_distance);
        if (last.m_distance == seg.m_distance)
        {
            assert(!required);
            return;
        }
    }
    array.push_back(seg);
}

// These add[SegmentType]Segs routines append intermediate segments for the curve.
// They assume the caller has set the initial segment (with t == 0), so they only
// add intermediates.

constexpr static int kMaxSegments = 100; // Arbirtary safety limit.

float ContourMeasureIter::addQuadSegs(std::vector<ContourMeasure::Segment>& segs,
                                      const Vec2D pts[],
                                      uint32_t ptIndex,
                                      float distance) const
{
    int count = static_cast<int>(ceilf(wangs_formula::quadratic(pts, m_invTolerance)));
    count = std::max(1, std::min(count, kMaxSegments));
    const float dt = 1.0f / count;
    const EvalQuad eval(pts);

    float t = dt;
    Vec2D prev = pts[0];
    for (int i = 1; i < count; ++i)
    {
        auto next = eval(t);
        distance += (next - prev).length();
        addSeg(segs, {distance, ptIndex, toDot30(t), SegmentType::kQuad});
        prev = next;
        t += dt;
    }
    distance += (pts[2] - prev).length();
    addSeg(segs, {distance, ptIndex, kMaxDot30, SegmentType::kQuad});
    return distance;
}

float ContourMeasureIter::addCubicSegs(std::vector<ContourMeasure::Segment>& segs,
                                       const Vec2D pts[],
                                       uint32_t ptIndex,
                                       float distance) const
{
    int count = static_cast<int>(ceilf(wangs_formula::cubic(pts, m_invTolerance)));
    count = std::max(1, std::min(count, kMaxSegments));
    const float dt = 1.0f / count;
    const EvalCubic eval(pts);

    float t = dt;
    Vec2D prev = pts[0];
    for (int i = 1; i < count; ++i)
    {
        auto next = eval(t);
        distance += (next - prev).length();
        addSeg(segs, {distance, ptIndex, toDot30(t), SegmentType::kCubic});
        prev = next;
        t += dt;
    }
    distance += (pts[3] - prev).length();
    addSeg(segs, {distance, ptIndex, kMaxDot30, SegmentType::kCubic});
    return distance;
}

void ContourMeasureIter::rewind(const RawPath& path, float tolerance)
{
    m_iter = path.begin();
    m_end = path.end();
    m_srcPoints = path.points().data();

    constexpr float kMinTolerance = 1.0f / 16;
    m_invTolerance = 1.0f / std::max(tolerance, kMinTolerance);
}

// Can return null if either it encountered an empty contour (length == 0)
// or the iterator is exhausted.
//
rcp<ContourMeasure> ContourMeasureIter::tryNext()
{
    std::vector<ContourMeasure::Segment> segs;
    std::vector<Vec2D> pts;
    float distance = 0;
    bool isClosed = false;

    for (; m_iter != m_end; ++m_iter)
    {
        PathVerb verb = std::get<0>(*m_iter);
        const Vec2D* iterPts = std::get<1>(*m_iter);
        if (verb == PathVerb::move)
        {
            if (!pts.empty())
            {
                break; // We've alredy seen a move. Save this one for next time.
            }
            pts.push_back(iterPts[0]);
            continue;
        }
        assert(!pts.empty()); // PathVerb::move should have occurred first, and added a point.
        assert(!isClosed);    // PathVerb::close is always followed by a move or nothing.
        float prevDistance = distance;
        const uint32_t ptIndex = castTo<uint32_t>(pts.size() - 1);
        switch (verb)
        {
            case PathVerb::line:
                distance += (iterPts[1] - iterPts[0]).length();
                if (distance > prevDistance)
                {
                    addSeg(segs, {distance, ptIndex, kMaxDot30, SegmentType::kLine}, true);
                    pts.push_back(iterPts[1]);
                }
                break;
            case PathVerb::quad:
                distance = this->addQuadSegs(segs, iterPts, ptIndex, distance);
                if (distance > prevDistance)
                {
                    pts.push_back(iterPts[1]);
                    pts.push_back(iterPts[2]);
                }
                break;
            case PathVerb::cubic:
                distance = this->addCubicSegs(segs, iterPts, ptIndex, distance);
                if (distance > prevDistance)
                {
                    pts.push_back(iterPts[1]);
                    pts.push_back(iterPts[2]);
                    pts.push_back(iterPts[3]);
                }
                break;
            case PathVerb::close:
            {
                auto first = pts.front();
                distance += (first - iterPts[0]).length();
                if (distance > prevDistance)
                {
                    addSeg(segs, {distance, ptIndex, kMaxDot30, SegmentType::kLine}, true);
                    pts.push_back(first);
                }
                isClosed = true;
            }
            break;
            case PathVerb::move:
                RIVE_UNREACHABLE(); // Handled above.
        }
    }

    if (distance == 0 || pts.size() < 2)
    {
        return nullptr;
    }
    return rcp<ContourMeasure>(
        new ContourMeasure(std::move(segs), std::move(pts), distance, isClosed));
}

rcp<ContourMeasure> ContourMeasureIter::next()
{
    rcp<ContourMeasure> cm;
    for (;;)
    {
        if ((cm = this->tryNext()))
        {
            break;
        }
        if (m_iter == m_end)
        {
            break;
        }
    }
    return cm;
}
