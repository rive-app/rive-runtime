#include "rive/tess/contour_render_path.hpp"
#include "rive/tess/contour_stroke.hpp"

using namespace rive;

PathCommand::PathCommand(PathCommandType type) : m_Type(type) {}
PathCommand::PathCommand(PathCommandType type, float x, float y) : m_Type(type), m_Point(x, y) {}
PathCommand::PathCommand(
    PathCommandType type, float outX, float outY, float inX, float inY, float x, float y) :
    m_Type(type), m_OutPoint(outX, outY), m_InPoint(inX, inY), m_Point(x, y) {}

void ContourRenderPath::addRenderPath(RenderPath* path, const Mat2D& transform) {
    m_SubPaths.emplace_back(SubPath(path, transform));
}

void ContourRenderPath::reset() {
    m_IsClosed = false;
    m_SubPaths.clear();
    m_ContourVertices.clear();
    m_Commands.clear();
    m_IsDirty = true;
}

void ContourRenderPath::moveTo(float x, float y) {
    m_Commands.emplace_back(PathCommand(PathCommandType::move, x, y));
}

void ContourRenderPath::lineTo(float x, float y) {
    m_Commands.emplace_back(PathCommand(PathCommandType::line, x, y));
}

void ContourRenderPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y) {
    m_Commands.emplace_back(PathCommand(PathCommandType::cubic, ox, oy, ix, iy, x, y));
}
void ContourRenderPath::close() {
    m_Commands.emplace_back(PathCommand(PathCommandType::close));
    m_IsClosed = true;
}

bool ContourRenderPath::isContainer() const { return !m_SubPaths.empty(); }

void ContourRenderPath::extrudeStroke(ContourStroke* stroke,
                                      StrokeJoin join,
                                      StrokeCap cap,
                                      float strokeWidth,
                                      const Mat2D& transform) {
    if (isContainer()) {
        for (auto& subPath : m_SubPaths) {
            static_cast<ContourRenderPath*>(subPath.path())
                ->extrudeStroke(stroke, join, cap, strokeWidth, subPath.transform());
        }
        return;
    }

    if (isDirty()) {
        computeContour();
    }

    stroke->extrude(this, m_IsClosed, join, cap, strokeWidth, transform);
}