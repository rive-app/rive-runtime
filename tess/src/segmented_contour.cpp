#include "rive/tess/segmented_contour.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/cubic_utilities.hpp"

using namespace rive;

SegmentedContour::SegmentedContour(float threshold) :
    m_threshold(threshold), m_thresholdSquared(threshold * threshold) {}

float SegmentedContour::threshold() const { return m_threshold; }
void SegmentedContour::threshold(float value) {
    m_threshold = value;
    m_thresholdSquared = value * value;
}
const AABB& SegmentedContour::bounds() const { return m_bounds; }
void SegmentedContour::addVertex(Vec2D vertex) {}
void SegmentedContour::penDown() {
    if (m_isPenDown) {
        return;
    }
    m_isPenDown = true;
    m_penDown = m_pen;
    addVertex(m_penDown);
}
void SegmentedContour::close() {
    if (!m_isPenDown) {
        return;
    }
    m_pen = m_penDown;
    m_isPenDown = false;

    // TODO: Can we optimize and not dupe this point if it's the last point
    // already in the list? For example: a procedural triangle closes itself
    // with a lineTo the first point.
    addVertex(m_penDown);
}

const Span<const Vec2D> SegmentedContour::contourPoints() const {
    return Span<const Vec2D>(m_contourPoints.data(), m_contourPoints.size());
}

void SegmentedContour::segmentCubic(const Vec2D& from,
                                    const Vec2D& fromOut,
                                    const Vec2D& toIn,
                                    const Vec2D& to,
                                    float t1,
                                    float t2) {
    if (CubicUtilities::shouldSplitCubic(from, fromOut, toIn, to, m_threshold)) {
        float halfT = (t1 + t2) / 2.0f;

        Vec2D hull[6];
        CubicUtilities::computeHull(from, fromOut, toIn, to, 0.5f, hull);

        segmentCubic(from, hull[0], hull[3], hull[5], t1, halfT);

        segmentCubic(hull[5], hull[4], hull[2], to, halfT, t2);
    } else {
        if (Vec2D::distanceSquared(from, to) > m_thresholdSquared) {
            addVertex(Vec2D(CubicUtilities::cubicAt(t2, from.x, fromOut.x, toIn.x, to.x),
                            CubicUtilities::cubicAt(t2, from.y, fromOut.y, toIn.y, to.y)));
        }
    }
}

void SegmentedContour::contour(const RawPath& rawPath) {
    m_contourPoints.clear();

    // First four vertices are the bounds.
    m_contourPoints.emplace_back(Vec2D());
    m_contourPoints.emplace_back(Vec2D());
    m_contourPoints.emplace_back(Vec2D());
    m_contourPoints.emplace_back(Vec2D());

    RawPath::Iter iter(rawPath);
    while (auto rec = iter.next()) {
        switch (rec.verb) {
            case PathVerb::move:
                m_isPenDown = false;
                m_pen = rec.pts[0];
                break;
            case PathVerb::line:
                penDown();
                m_pen = rec.pts[0];
                addVertex(rec.pts[0]);
                break;
            case PathVerb::cubic:
                penDown();
                segmentCubic(m_pen, rec.pts[0], rec.pts[1], rec.pts[2], 0.0f, 1.0f);
                m_pen = rec.pts[2];
                break;
            case PathVerb::close:
                close();
                break;
            case PathVerb::quad:
                // TODO: not currently used by render paths, however might be
                // necessary for fonts.
                break;
        }
    }

    // TODO: when we stroke we may want to differentiate whether or not the path
    // actually closed.
    close();

    // TODO: consider if there's a case with no points.
    Vec2D& first = m_contourPoints[0];
    first.x = m_bounds.minX;
    first.y = m_bounds.minY;

    Vec2D& second = m_contourPoints[1];
    second.x = m_bounds.maxX;
    second.y = m_bounds.minY;

    Vec2D& third = m_contourPoints[2];
    third.x = m_bounds.maxX;
    third.y = m_bounds.maxY;

    Vec2D& fourth = m_contourPoints[3];
    fourth.x = m_bounds.minX;
    fourth.y = m_bounds.maxY;
}
