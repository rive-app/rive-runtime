#ifndef _RIVE_TESS_RENDER_PATH_HPP_
#define _RIVE_TESS_RENDER_PATH_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/span.hpp"
#include "rive/tess/segmented_contour.hpp"
#include "rive/tess/sub_path.hpp"
#include "earcut.hpp"

namespace rive
{

class ContourStroke;
class TessRenderPath : public lite_rtti_override<RenderPath, TessRenderPath>
{
private:
    // TessRenderPath stores a RawPath so that it can use utility classes
    // that will work off of RawPath (like segmenting the contour and then
    // triangulating the segmented contour).
    RawPath m_rawPath;
    FillRule m_fillRule;

    // We hold a reference to the segmented contour so it can reserve and
    // reuse storage when re-contouring.
    SegmentedContour m_segmentedContour;

    mapbox::detail::Earcut<uint16_t> m_earcut;

    bool m_isContourDirty = true;
    bool m_isTriangulationDirty = true;
    Mat2D m_contourTransform;
    bool m_isClosed = false;

protected:
    std::vector<SubPath> m_subPaths;
    virtual void addTriangles(Span<const Vec2D> vertices, Span<const uint16_t> indices) = 0;
    virtual void setTriangulatedBounds(const AABB& value) = 0;
    void contour(const Mat2D& transform);
    void triangulate(TessRenderPath* containerPath);

public:
    TessRenderPath();
    TessRenderPath(RawPath&, FillRule);
    ~TessRenderPath();
    void rewind() override;
    void fillRule(FillRule value) override;
    bool empty() const;

    // In Rive a Path is used as a simple container (with no commands) when
    // it aggregates multiple paths.
    bool isContainer() const { return !m_subPaths.empty(); }

    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
    void close() override;
    void addRenderPath(RenderPath* path, const Mat2D& transform) override;

    const SegmentedContour& segmentedContour() const;
    bool triangulate();
    void extrudeStroke(ContourStroke* stroke,
                       StrokeJoin join,
                       StrokeCap cap,
                       float strokeWidth,
                       const Mat2D& transform);
    const RawPath& rawPath() const;
};
} // namespace rive
#endif
