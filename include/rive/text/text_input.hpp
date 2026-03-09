#ifndef _RIVE_TEXT_INPUT_HPP_
#define _RIVE_TEXT_INPUT_HPP_

#include "rive/generated/text/text_input_base.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/input/focusable.hpp"

namespace rive
{
class TextStyle;
class ScrollConstraint;
class TextInput : public TextInputBase, public TextInterface, public Focusable
{
public:
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    bool hitTestPoint(const Vec2D& position,
                      bool skipOnUnclipped,
                      bool isPrimaryHit) override;

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

    bool keyInput(Key value,
                  KeyModifiers modifiers,
                  bool isPressed,
                  bool isRepeat) override;
    bool textInput(const std::string& text) override;
    void focused() override;
    void blurred() override;
    bool worldPosition(Vec2D& outPosition) override;
    bool worldBounds(AABB& outBounds) override;
    Artboard* focusableArtboard() const override { return artboard(); }

    /// Called when the user starts dragging on the text input.
    /// Places the cursor at the given world position.
    void startDrag(Vec2D worldPosition);

    /// Called when the user continues dragging on the text input.
    /// Extends the selection to the given world position.
    void drag(Vec2D worldPosition);

    /// Called when the user ends dragging on the text input.
    void endDrag(Vec2D worldPosition);

    /// Advance edge scrolling during drag. Returns true if still scrolling.
    bool advanceDrag(float elapsedSeconds);

    /// Whether currently dragging (for hit test to avoid interference).
    bool isDragging() const { return m_isDragging; }

protected:
    void textChanged() override;
    void selectionRadiusChanged() override;

private:
    /// Convert a world position to local text input coordinates.
    /// Handles viewport clamping and auto-scroll for scroll constraints.
    bool worldToLocalWithViewport(Vec2D worldPosition, Vec2D& outLocal);

    AABB m_worldBounds;
    TextStyle* m_textStyle = nullptr;
    ScrollConstraint* m_scrollConstraint = nullptr;

    /// Whether the user is currently dragging to select text.
    bool m_isDragging = false;

    /// Scroll velocity for edge scrolling during drag.
    float m_scrollY = 0.0f;

#ifdef WITH_RIVE_TEXT
    RawTextInput m_rawTextInput;
#endif
};
} // namespace rive

#endif