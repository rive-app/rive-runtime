#ifndef _RIVE_TEXT_INPUT_DRAWABLE_HPP_
#define _RIVE_TEXT_INPUT_DRAWABLE_HPP_

#include "rive/generated/text/text_input_drawable_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"

namespace rive
{
class TextInput;
class TextInputDrawable : public TextInputDrawableBase,
                          public ShapePaintContainer
{
private:
    Artboard* getArtboard() override { return artboard(); }

public:
    ShapePaintPath* worldPath() override;
    const Mat2D& shapeWorldTransform() const override;
    ShapePaintPath* localPath() override { return localClockwisePath(); }
    StatusCode onAddedClean(CoreContext* context) override;
    TextInput* textInput() const;
    Component* pathBuilder() override { return parent(); }
    void draw(Renderer* renderer) override;
    bool willDraw() override;
};
} // namespace rive

#endif