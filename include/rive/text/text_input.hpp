#ifndef _RIVE_TEXT_INPUT_HPP_
#define _RIVE_TEXT_INPUT_HPP_

#include "rive/generated/text/text_input_base.hpp"
#include "rive/advancing_component.hpp"
#include "rive/text/raw_text_input.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/input/focusable.hpp"
#include <cmath>

namespace rive
{
class TextStyle;
class ScrollConstraint;
class TextInput : public TextInputBase,
                  public TextInterface,
                  public Focusable,
                  public AdvancingComponent
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
    std::string selectedText() const override;
    bool gamepadDispatch(
        const ListenerInvocation& invocation,
        ScriptedDrawable** outDispatchedScriptedDrawable = nullptr) override;
    void focused() override;
    void blurred() override;
    bool worldPosition(Vec2D& outPosition) override;
    bool worldBounds(AABB& outBounds) override;
    Artboard* focusableArtboard() const override { return artboard(); }
    bool acceptsKeyboardInput() const override { return true; }

    /// Called when the user starts dragging on the text input.
    /// Places the cursor at the given world position.
    void startDrag(Vec2D worldPosition);

    /// Called when the user continues dragging on the text input.
    /// Extends the selection to the given world position.
    void drag(Vec2D worldPosition);

    /// Called when the user ends dragging on the text input.
    void endDrag(Vec2D worldPosition);

    /// Selects the word at the current cursor (e.g. on double-click).
    void selectWord();

    /// Selects the visual line at the current cursor (e.g. on triple-click).
    void selectLine();

    /// Advance edge scrolling during drag. Returns true if still scrolling.
    bool advanceDrag(float elapsedSeconds);
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;

    /// Whether currently dragging (for hit test to avoid interference).
    bool isDragging() const { return m_isDragging; }

    /// Whether this text input currently has focus.
    bool isFocused() const { return m_focused; }

    /// Whether the caret should be drawn this frame. Only true while focused
    /// and while the blink cycle is in its visible phase.
    bool isCursorVisible() const { return m_focused && m_cursorBlinkVisible; }

protected:
    void textChanged() override;
    void selectionRadiusChanged() override;
    void multilineChanged() override;
    void alignValueChanged() override;
    void verticalAlignValueChanged() override;

private:
    /// Push the current alignment and the width to align it within down to the
    /// raw text input. Returns true if either actually changed.
    bool updateAlignment();

    /// Convert a world position to local text input coordinates.
    /// Handles viewport clamping and auto-scroll for scroll constraints.
    bool worldToLocalWithViewport(Vec2D worldPosition,
                                  Vec2D& outLocal,
                                  bool enableAutoScroll);

    float edgeScrollSpeedForDistance(float distanceFromEdge) const;
    float edgeActivationDistance(float position, float edgeStart) const;

    /// Show the caret and restart its blink cycle, so it stays solid while the
    /// user is typing or moving it around.
    void restartCursorBlink();

    /// Toggle the caret when the blink interval elapses. Returns true while
    /// focused so frames keep coming and the caret keeps blinking.
    bool advanceCursorBlink(float elapsedSeconds);

    void updateMultiline(bool syncDisplayedText = false);
    static std::string strippedLineBreaks(const std::string& text);
    std::string displayedText() const;
    void syncDisplayedTextFromSource(bool preserveCursor);
    void syncSourceTextFromRaw();

    AABB m_worldBounds;
    std::string m_sourceText;
    TextStyle* m_textStyle = nullptr;
    ScrollConstraint* m_scrollConstraint = nullptr;

    /// Whether the user is currently dragging to select text.
    bool m_isDragging = false;

    /// Whether this text input currently has focus.
    bool m_focused = false;

    /// Whether the caret is in the shown phase of its blink cycle.
    bool m_cursorBlinkVisible = true;

    /// Time spent in the current phase of the caret's blink cycle.
    float m_cursorBlinkSeconds = 0.0f;
    Vec2D m_lastDragWorldPosition = Vec2D(NAN, NAN);

    /// Scroll velocity for edge scrolling during drag in X.
    float m_scrollX = 0.0f;

    /// Scroll velocity for edge scrolling during drag.
    float m_scrollY = 0.0f;
    float m_layoutWidth = NAN;
    float m_layoutHeight = NAN;

#ifdef WITH_RIVE_TEXT
    RawTextInput m_rawTextInput;
#endif
};
} // namespace rive

#endif