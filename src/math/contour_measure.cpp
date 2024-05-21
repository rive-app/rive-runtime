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
    assert(m_segments.front().m_distance >= 0);
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
    // When t==0 and t==1, the most accurate way to find tangents is by differencing.
    if (t == 0 || t == 1)
    {
        if (t == 0)
        {
            return {pts[0],
                    (pts[0] != pts[1]   ? pts[1]
                     : pts[1] != pts[2] ? pts[2]
                                        : pts[3]) -
                        pts[0]

            };
        }
        else
        {
            return {pts[3],
                    pts[3] - (pts[3] != pts[2]   ? pts[2]
                              : pts[2] != pts[1] ? pts[1]
                                                 : pts[0])};
        }
    }

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
    if (distance > m_length)
    {
        distance = m_length;
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

    assert(prevDist <= seg.m_distance);
    const auto ratio = (distance - prevDist) / (seg.m_distance - prevDist);
    float t = lerp(prevT, seg.getT(), ratio);
    t = math::clamp(t, prevT, seg.getT());
    assert(prevT <= t && t <= seg.getT());
    return t;
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

// These add[SegmentType]Segs routines append intermediate segments for the curve.
// They assume the caller has set the initial segment (with t == 0), so they only
// add intermediates.

float ContourMeasureIter::addQuadSegs(ContourMeasure::Segment* segs,
                                      const Vec2D pts[],
                                      uint32_t segmentCount,
                                      uint32_t ptIndex,
                                      float distance) const
{
    const float dt = 1.f / segmentCount;
    const EvalQuad eval(pts);

    float t = dt;
    Vec2D prev = pts[0];
    for (uint32_t i = 1; i < segmentCount; ++i)
    {
        auto next = eval(t);
        distance += (next - prev).length();
        *segs++ = {distance, ptIndex, toDot30(t), SegmentType::kQuad};
        prev = next;
        t += dt;
    }
    distance += (pts[2] - prev).length();
    *segs++ = {distance, ptIndex, kMaxDot30, SegmentType::kQuad};
    return distance;
}

float ContourMeasureIter::addCubicSegs(ContourMeasure::Segment* segs,
                                       const Vec2D pts[],
                                       uint32_t segmentCount,
                                       uint32_t ptIndex,
                                       float distance) const
{
    const float dt = 1.f / segmentCount;
    const EvalCubic eval(pts);

    float t = dt;
    Vec2D prev = pts[0];
    for (uint32_t i = 1; i < segmentCount; ++i)
    {
        auto next = eval(t);
        distance += (next - prev).length();
        *segs++ = {distance, ptIndex, toDot30(t), SegmentType::kCubic};
        prev = next;
        t += dt;
    }
    distance += (pts[3] - prev).length();
    *segs++ = {distance, ptIndex, kMaxDot30, SegmentType::kCubic};
    return distance;
}

void ContourMeasureIter::rewind(const RawPath* path, float tolerance)
{
    m_iter = path->begin();
    m_end = path->end();
    m_srcPoints = path->points().data();

    constexpr float kMinTolerance = 1.0f / 16;
    m_invTolerance = 1.0f / std::max(tolerance, kMinTolerance);

    m_segmentCounts.resize(path->verbs().count());
}

// Can return null if either it encountered an empty contour (length == 0)
// or the iterator is exhausted.
//
rcp<ContourMeasure> ContourMeasureIter::tryNext()
{
    assert(m_iter == m_end || m_iter.verb() == PathVerb::move);

    // Advance to the first line or curve.
    Vec2D p0;
    for (;; ++m_iter)
    {
        if (m_iter == m_end)
        {
            return nullptr;
        }
        switch (m_iter.verb())
        {
            case PathVerb::move:
                p0 = m_iter.movePt();
                RIVE_FALLTHROUGH;
            case PathVerb::close:
                continue;
            default:
                break;
        }
        break;
    }

    // Pass 1: count segments.
    uint32_t* nextSegCount = m_segmentCounts.data();
    size_t segmentCountInCurves = 0;
    size_t lineCount = 0;
    bool isClosed = false;
    RawPath::Iter endOfContour = m_end;
    for (auto it = m_iter; it != m_end; ++it)
    {
        // Arbirtary limit to keep our segmenting tractable.
        constexpr static uint32_t kMaxSegments = 100;
        switch (it.verb())
        {
            case PathVerb::move:
                endOfContour = it; // This move belongs to the next contour.
                break;
            case PathVerb::line:
                ++lineCount;
                continue;
            case PathVerb::quad:
            {
                assert(nextSegCount < m_segmentCounts.data() + m_segmentCounts.size());
                uint32_t segmentCount = static_cast<uint32_t>(
                    ceilf(wangs_formula::quadratic(it.quadPts(), m_invTolerance)));
                segmentCount = std::max(1u, std::min(segmentCount, kMaxSegments));
                segmentCountInCurves += segmentCount;
                *nextSegCount++ = segmentCount;
                continue;
            }
            case PathVerb::cubic:
            {
                assert(nextSegCount < m_segmentCounts.data() + m_segmentCounts.size());
                uint32_t segmentCount = static_cast<uint32_t>(
                    ceilf(ceilf(wangs_formula::cubic(it.cubicPts(), m_invTolerance))));
                segmentCount = std::max(1u, std::min(segmentCount, kMaxSegments));
                segmentCountInCurves += segmentCount;
                *nextSegCount++ = segmentCount;
                continue;
            }
            case PathVerb::close:
                if (it.ptBeforeClose() != p0)
                {
                    ++lineCount;
                }
                isClosed = true;
                continue;
        }
        break;
    }

    // Pass 2: emit segments.
    std::vector<ContourMeasure::Segment> segs;
    segs.resize(segmentCountInCurves + lineCount);
    ContourMeasure::Segment* nextSeg = segs.data();
    nextSegCount = m_segmentCounts.data();
    float distance = 0;
    uint32_t ptIndex = 0;
    bool duplicateP0 = false;
    for (auto it = m_iter; it != endOfContour; ++it)
    {
        switch (it.verb())
        {
            case PathVerb::move:
                RIVE_UNREACHABLE();
            case PathVerb::line:
                distance += (it.linePts()[1] - it.linePts()[0]).length();
                *nextSeg++ = {distance, ptIndex, kMaxDot30, SegmentType::kLine};
                ++ptIndex;
                break;
            case PathVerb::quad:
            {
                const uint32_t n = *nextSegCount++;
                distance = addQuadSegs(nextSeg, it.quadPts(), n, ptIndex, distance);
                nextSeg += n;
                ptIndex += 2;
                break;
            }
            case PathVerb::cubic:
            {
                const uint32_t n = *nextSegCount++;
                distance = addCubicSegs(nextSeg, it.cubicPts(), n, ptIndex, distance);
                nextSeg += n;
                ptIndex += 3;
                break;
            }
            case PathVerb::close:
                if (it.ptBeforeClose() != p0)
                {
                    distance += (p0 - it.ptBeforeClose()).length();
                    *nextSeg++ = {distance, ptIndex, kMaxDot30, SegmentType::kLine};
                    ++ptIndex;
                    duplicateP0 = true;
                }
                assert(isClosed);
        }
    }
    assert(nextSeg == segs.data() + segs.size());

    // Copy out points.
    std::vector<Vec2D> pts;
    pts.reserve(1 + ptIndex);
    pts.insert(pts.end(), m_iter.rawPtsPtr() - 1, endOfContour.rawPtsPtr());
    if (duplicateP0)
    {
        pts.push_back(p0);
    }
    assert(pts.size() == 1 + ptIndex);

    m_iter = endOfContour;

    if (distance > 0 && pts.size() >= 2)
    {
        assert(!std::isnan(distance));
        return rcp<ContourMeasure>(
            new ContourMeasure(std::move(segs), std::move(pts), distance, isClosed));
    }

    assert(distance == 0 || std::isnan(distance));
    return nullptr;
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
    assert(!cm || !std::isnan(cm->length()));
    return cm;
}
