#ifndef _RIVE_TEXT_STYLE_BACKGROUND_HPP_
#define _RIVE_TEXT_STYLE_BACKGROUND_HPP_
#include "rive/generated/text/text_style_background_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/text_selection_path.hpp"
#include <vector>

namespace rive
{
class TextStylePaint;

/// Paints a joined rounded rectangle behind the glyphs of every run using the
/// owning TextStylePaint.
class TextStyleBackground : public TextStyleBackgroundBase,
                            public ShapePaintContainer
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    void buildDependencies() override;

    void resetPath();
    void addRect(const AABB& rect);
    void updatePath();
    void draw(Renderer* renderer, const Mat2D& worldTransform);

    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override;
    Component* pathBuilder() override;
    ShapePaintPath* localPath() override { return &m_path; }
    ShapePaintPath* localClockwisePath() override { return &m_path; }

protected:
    void cornerRadiusChanged() override;

private:
    Artboard* getArtboard() override { return artboard(); }
    TextStylePaint* style() const;

    std::vector<AABB> m_rects;
    // TextSelectionPath winds outer contours clockwise and holes
    // counter-clockwise, so the clockwise fill rule is accurate here -- and
    // the renderer only feathers fills whose path is clockwise.
    TextSelectionPath m_path{true, FillRule::clockwise};
};
} // namespace rive

#endif
