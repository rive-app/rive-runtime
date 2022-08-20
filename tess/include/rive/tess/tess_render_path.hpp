#ifndef _RIVE_TESS_RENDER_PATH_HPP_
#define _RIVE_TESS_RENDER_PATH_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/span.hpp"

namespace rive {
class TessRenderPath : public RenderPath {
private:
    // TessRenderPath stores a RawPath so that it can use utility classes
    // that will work off of RawPath (like segmenting the contour and then
    // triangulating the segmented contour).
    RawPath m_RawPath;
    FillRule m_FillRule;

public:
    TessRenderPath();
    TessRenderPath(Span<const Vec2D> points, Span<const PathVerb> verbs, FillRule fillRule);
    ~TessRenderPath();
    void reset() override;
    void fillRule(FillRule value) override;

    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
    void close() override;
};
} // namespace rive
#endif