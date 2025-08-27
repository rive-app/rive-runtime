#ifndef _RIVE_TEXT_INPUT_HPP_
#define _RIVE_TEXT_INPUT_HPP_

#include "rive/generated/text/text_input_base.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive/text/text_interface.hpp"

namespace rive
{
class TextStyle;
class TextInput : public TextInputBase, public TextInterface
{
public:
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;

#ifdef WITH_RIVE_TEXT
    RawTextInput* rawTextInput() { return &m_rawTextInput; }
#endif

    void markPaintDirty() override;
    void markShapeDirty() override;
    StatusCode onAddedClean(CoreContext* context) override;

    AABB localBounds() const override;
    void update(ComponentDirt value) override;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;

protected:
    void textChanged() override;
    void selectionRadiusChanged() override;

private:
    AABB m_worldBounds;
    TextStyle* m_textStyle = nullptr;
#ifdef WITH_RIVE_TEXT
    RawTextInput m_rawTextInput;
#endif
};
} // namespace rive

#endif