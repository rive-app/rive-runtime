#include "rive/tess/segmented_contour.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/cubic_utilities.hpp"

using namespace rive;

SegmentedContour::SegmentedContour(float threshold) :
    m_bounds(AABB::forExpansion()),
    m_threshold(threshold),
    m_thresholdSquared(threshold * threshold) {}

float SegmentedContour::threshold() const { return m_threshold; }
void SegmentedContour::threshold(float value) {
    m_threshold = value;
    m_thresholdSquared = value * value;
}
const AABB& SegmentedContour::bounds() const { return m_bounds; }
void SegmentedContour::addVertex(Vec2D vertex) {
    m_contourPoints.push_back(vertex);
    AABB::expandTo(m_bounds, vertex);
}

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
}

const std::size_t SegmentedContour::contourSize() const { return m_contourPoints.size(); }

const Span<const Vec2D> SegmentedContour::contourPoints(uint32_t endOffset) const {
    assert(endOffset <= m_contourPoints.size());
    return Span<const Vec2D>(m_contourPoints.data(), m_contourPoints.size() - endOffset);
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

void SegmentedContour::contour(const RawPath& rawPath, const Mat2D& transform) {
    m_contourPoints.clear();

    RawPath::Iter iter(rawPath);
    // Possible perf consideration: could add second path that doesn't transform
    // if transform is the identity.
    while (auto rec = iter.next()) {
        switch (rec.verb) {
            case PathVerb::move:
                m_isPenDown = false;
                m_pen = transform * rec.pts[0];
                break;
            case PathVerb::line:
                penDown();
                m_pen = transform * rec.pts[0];
                addVertex(m_pen);
                break;
            case PathVerb::cubic:
                penDown();
                segmentCubic(m_pen,
                             transform * rec.pts[0],
                             transform * rec.pts[1],
                             transform * rec.pts[2],
                             0.0f,
                             1.0f);
                m_pen = transform * rec.pts[2];
                break;
            case PathVerb::close: close(); break;
            case PathVerb::quad:
                // TODO: not currently used by render paths, however might be
                // necessary for fonts.
                break;
        }
    }
    close();
}
