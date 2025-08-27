#ifndef _RIVE_TEXT_STYLE_PAINT_HPP_
#define _RIVE_TEXT_STYLE_PAINT_HPP_
#include "rive/generated/text/text_style_paint_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include <unordered_map>

namespace rive
{
class TextStylePaint : public TextStylePaintBase, public ShapePaintContainer
{
public:
    TextStylePaint();
    bool addPath(const RawPath& rawPath, float opacity);
    void rewindPath();
    void draw(Renderer* renderer, const Mat2D& worldTransform);

    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override;
    Component* pathBuilder() override;
    ShapePaintPath* localPath() override { return &m_path; }
    ShapePaintPath* localClockwisePath() override { return &m_path; }
    Core* clone() const override;

private:
    Artboard* getArtboard() override { return artboard(); }
    std::unordered_map<float, ShapePaintPath> m_opacityPaths;
    std::vector<rcp<RenderPaint>> m_paintPool;
    ShapePaintPath m_path;
    bool m_hasContents = false;
};
} // namespace rive

#endif