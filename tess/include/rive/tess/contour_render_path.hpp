#ifndef _RIVE_CONTOUR_RENDER_PATH_HPP_
#define _RIVE_CONTOUR_RENDER_PATH_HPP_

#include "rive/renderer.hpp"
#include "rive/math/aabb.hpp"
#include "rive/tess/sub_path.hpp"
#include <vector>
#include <cstdint>

namespace rive {
    enum class PathCommandType : uint8_t {
        /// Corresponds to CommandPath::moveTo
        move,
        /// Corresponds to CommandPath::lineTo
        line,
        /// Corresponds to CommandPath::cubicTo
        cubic,
        /// Corresponds to CommandPath::close
        close
    };

    class PathCommand {
    private:
        PathCommandType m_Type;
        /// Only used when m_Type is cubic.
        Vec2D m_OutPoint;

        /// Only used when m_Type is cubic.
        Vec2D m_InPoint;

        /// Only used when m_Type is move or close or cubic.
        Vec2D m_Point;

    public:
        PathCommand(PathCommandType type);
        PathCommand(PathCommandType type, float x, float y);
        PathCommand(
            PathCommandType type, float outX, float outY, float inX, float inY, float x, float y);

        PathCommandType type() const { return m_Type; }
        const Vec2D& outPoint() const { return m_OutPoint; }
        const Vec2D& inPoint() const { return m_InPoint; }
        const Vec2D& point() const { return m_Point; }
    };

    class ContourStroke;
    ///
    /// Segments curves into line segments and computes the bounds of the
    /// segmented curve.
    ///
    class ContourRenderPath : public RenderPath {
    protected:
        AABB m_ContourBounds;
        std::vector<Vec2D> m_ContourVertices;
        std::vector<SubPath> m_SubPaths;
        std::vector<PathCommand> m_Commands;
        bool m_IsDirty = true;
        float m_ContourThreshold = 1.0f;
        bool m_IsClosed = false;

    public:
        std::size_t contourLength() const { return m_ContourVertices.size(); }
        const std::vector<Vec2D>& contourVertices() const { return m_ContourVertices; }
        bool isClosed() const { return m_IsClosed; }

        bool isContainer() const;
        void addRenderPath(RenderPath* path, const Mat2D& transform) override;

        void reset() override;
        void moveTo(float x, float y) override;
        void lineTo(float x, float y) override;
        void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
        void close() override;

        void computeContour();
        bool isDirty() const { return m_IsDirty; }

        void extrudeStroke(ContourStroke* stroke,
                           StrokeJoin join,
                           StrokeCap cap,
                           float strokeWidth,
                           const Mat2D& transform);
    };
} // namespace rive
#endif