/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_CONTOUR_MEASURE_HPP_
#define _RIVE_CONTOUR_MEASURE_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/refcnt.hpp"
#include <utility>

namespace rive
{

class ContourMeasure : public RefCnt<ContourMeasure>
{
public:
    static constexpr unsigned kMaxDot30 = (1 << 30) - 1;
    static constexpr float kInvScaleD30 = 1.0f / (float)kMaxDot30;

    // Deliberately making this pack well (12 bytes)
    struct Segment
    {
        float m_distance;       // total distance up to this point
        uint32_t m_ptIndex;     // index of the first point for this line/quad/cubic
        unsigned m_tValue : 30; // Dot30 t value for the end of this segment
        unsigned m_type : 2;    // [private enum]

        float getT() const { return m_tValue * kInvScaleD30; }

        bool operator<(const Segment& other) const { return m_distance < other.m_distance; }

        void extract(RawPath* dst, float fromT, float toT, const Vec2D pts[], bool moveTo) const;
        void extract(RawPath* dst, const Vec2D pts[]) const;
    };

private:
    size_t findSegment(float distance) const;

    std::vector<Segment> m_segments;
    std::vector<Vec2D> m_points;
    const float m_length;
    const bool m_isClosed;

    ContourMeasure(std::vector<Segment>&&, std::vector<Vec2D>&&, float length, bool isClosed);

    friend class ContourMeasureIter;

public:
    float length() const { return m_length; }
    bool isClosed() const { return m_isClosed; }

    struct PosTan
    {
        Vec2D pos, tan;
    };
    PosTan getPosTan(float distance) const;

    void getSegment(float startDistance, float endDistance, RawPath* dst, bool startWithMove) const;

    Vec2D warp(Vec2D src) const
    {
        const auto result = this->getPosTan(src.x);
        return {
            result.pos.x - result.tan.y * src.y,
            result.pos.y + result.tan.x * src.y,
        };
    }

    void dump() const;
};

class ContourMeasureIter
{
    RawPath m_optionalCopy;
    RawPath::Iter m_iter;
    RawPath::Iter m_end;
    const Vec2D* m_srcPoints;
    float m_invTolerance;

    float addQuadSegs(ContourMeasure::Segment*,
                      const Vec2D[],
                      uint32_t segmentCount,
                      uint32_t ptIndex,
                      float distance) const;
    float addCubicSegs(ContourMeasure::Segment*,
                       const Vec2D[],
                       uint32_t segmentCount,
                       uint32_t ptIndex,
                       float distance) const;
    rcp<ContourMeasure> tryNext();

public:
    // Tolerance is the max deviation of the curve from its approximating line
    // segments. A smaller tolerance means more line segments, but a better
    // approximation for the curves actual length.
    static constexpr float kDefaultTolerance = 0.5f;

    ContourMeasureIter(const RawPath* path, float tol = kDefaultTolerance)
    {
        this->rewind(path, tol);
    }

    void rewind(const RawPath*, float = kDefaultTolerance);

    // Returns a measure object for each contour in the path
    //   (contours with zero-length are skipped over)
    // and then returns nullptr when its finished.
    //
    //  ContourMeasureIter iter(path);
    //  while ((auto meas = iter.next())) {
    //      ... meas can be used, and passed to other objects
    //  }
    //
    // Each measure object is stand-alone, and can outlive the ContourMeasureIter
    // that created it. It contains no back pointers to the Iter or to the path.
    //
    rcp<ContourMeasure> next();

    // Temporary storage used during tryNext(), for counting up how many segments a contour will be
    // divided into.
    std::vector<uint32_t> m_segmentCounts;
};

} // namespace rive

#endif
