#include "rive/tess/tess_render_path.hpp"

using namespace rive;
TessRenderPath::TessRenderPath() {}
TessRenderPath::TessRenderPath(Span<const Vec2D> points,
                               Span<const PathVerb> verbs,
                               FillRule fillRule) :
    m_RawPath(points.data(), points.size(), verbs.data(), verbs.size()), m_FillRule(fillRule) {}

TessRenderPath::~TessRenderPath() {}
void TessRenderPath::reset() { m_RawPath.reset(); }
void TessRenderPath::fillRule(FillRule value) {}

void TessRenderPath::moveTo(float x, float y) { m_RawPath.moveTo(x, y); }
void TessRenderPath::lineTo(float x, float y) { m_RawPath.lineTo(x, y); }
void TessRenderPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y) {
    m_RawPath.cubicTo(ox, oy, ix, iy, x, y);
}
void TessRenderPath::close() { m_RawPath.close(); }