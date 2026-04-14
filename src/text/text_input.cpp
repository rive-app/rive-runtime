#include "rive/text/text_input.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include <algorithm>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace rive;

void TextInput::draw(Renderer* renderer) {}

Core* TextInput::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

bool TextInput::hitTestPoint(const Vec2D& position,
                             bool skipOnUnclipped,
                             bool isPrimaryHit)
{
    // TextInput must check its own bounds before delegating to parent.
    // Unlike layouts, we always check bounds regardless of clip state.
    Mat2D inverseWorld;
    if (!worldTransform().invert(&inverseWorld))
    {
        return false;
    }

    Vec2D localPosition = inverseWorld * position;
    if (!localBounds().contains(localPosition))
    {
        return false;
    }

    // Bounds check passed, now check parent hierarchy
    return Drawable::hitTestPoint(position, skipOnUnclipped, isPrimaryHit);
}

void TextInput::textChanged()
{
    m_sourceText = m_Text;
#ifdef WITH_RIVE_TEXT
    syncDisplayedTextFromSource(false);
#endif
    markShapeDirty();
}
void TextInput::selectionRadiusChanged()
{
#ifdef WITH_RIVE_TEXT
    m_rawTextInput.selectionCornerRadius(selectionRadius());
#endif
}

void TextInput::multilineChanged() { updateMultiline(true); }

void TextInput::markPaintDirty() { addDirt(ComponentDirt::Paint); }

void TextInput::markShapeDirty() { addDirt(ComponentDirt::TextShape); }

AABB TextInput::localBounds() const
{
#ifdef WITH_RIVE_TEXT
    return m_rawTextInput.bounds();
#else
    return AABB();
#endif
}

StatusCode TextInput::onAddedClean(CoreContext* context)
{
    Super::onAddedClean(context);

    m_textStyle = children<TextStyle>().first();
    m_sourceText = m_Text;

#ifdef WITH_RIVE_TEXT
    if (m_textStyle != nullptr && m_textStyle->font() != nullptr)
    {
        m_rawTextInput.font(m_textStyle->font());
    }
    syncDisplayedTextFromSource(false);
#endif

    if (parent() != nullptr)
    {
        auto parentParent = parent()->parent();
        if (parentParent != nullptr && parentParent->is<TransformComponent>())
        {
            for (Constraint* constraint :
                 parentParent->as<TransformComponent>()->constraints())
            {
                if (constraint->is<ScrollConstraint>())
                {
                    m_scrollConstraint = constraint->as<ScrollConstraint>();
                    break;
                }
            }
        }
    }
    updateMultiline();
    return m_textStyle == nullptr ? StatusCode::MissingObject : StatusCode::Ok;
}

void TextInput::update(ComponentDirt value)
{
    Super::update(value);
#ifdef WITH_RIVE_TEXT
    if (hasDirt(value, ComponentDirt::Paint | ComponentDirt::TextShape))
    {
        Factory* factory = artboard()->factory();
        RawTextInput::Flags changed = m_rawTextInput.update(factory);
        if (enums::is_flag_set(changed, RawTextInput::Flags::shapeDirty))
        {
            m_worldBounds =
                worldTransform().mapBoundingBox(m_rawTextInput.bounds());

#ifdef WITH_RIVE_LAYOUT
            if (m_rawTextInput.sizing() == TextSizing::autoHeight)
            {
                markLayoutNodeDirty();
            }
#endif
        }
        if (enums::is_flag_set(changed, RawTextInput::Flags::selectionDirty))
        {
            for (auto child : children<TextInputDrawable>())
            {
                child->invalidateStrokeEffects();
            }
        }
        // Skip scroll adjustment during drag - edge scrolling handles it.
        if (m_scrollX == 0.0f && m_scrollY == 0.0f &&
            m_scrollConstraint != nullptr && !m_isDragging)
        {
            CursorVisualPosition cursorPosition =
                m_rawTextInput.cursorVisualPosition();
            float viewportWidth = m_scrollConstraint->viewportWidth();
            float viewportHeight = m_scrollConstraint->viewportHeight();
            const float cursorWidth = 1.0f;
            bool useHorizontalScroll =
                !multiline() && m_scrollConstraint->constrainsHorizontal();
            bool useVerticalScroll =
                multiline() && m_scrollConstraint->constrainsVertical();

            float viewportX =
                cursorPosition.x() + m_scrollConstraint->scrollOffsetX();
            float viewportTop =
                cursorPosition.top() + m_scrollConstraint->scrollOffsetY();
            float viewportBottom =
                cursorPosition.bottom() + m_scrollConstraint->scrollOffsetY();

            if (useHorizontalScroll && viewportX < 0.0f)
            {
                m_scrollConstraint->stopPhysics();

                float scrollOffset =
                    m_scrollConstraint->scrollOffsetX() - viewportX;
                m_scrollConstraint->scrollOffsetX(scrollOffset);
            }
            else if (useHorizontalScroll &&
                     viewportX > viewportWidth - cursorWidth)
            {
                m_scrollConstraint->stopPhysics();

                float scrollOffset = m_scrollConstraint->scrollOffsetX() -
                                     (viewportX - viewportWidth + cursorWidth);
                m_scrollConstraint->scrollOffsetX(scrollOffset);
            }

            if (useVerticalScroll && viewportTop < 0.0f)
            {
                m_scrollConstraint->stopPhysics();
                float scrollOffset =
                    m_scrollConstraint->scrollOffsetY() - viewportTop;
                m_scrollConstraint->scrollOffsetY(scrollOffset);
            }
            else if (useVerticalScroll && viewportBottom > viewportHeight)
            {
                m_scrollConstraint->stopPhysics();
                float scrollOffset = m_scrollConstraint->scrollOffsetY() -
                                     (viewportBottom - viewportHeight);
                m_scrollConstraint->scrollOffsetY(scrollOffset);
            }
        }
    }
#endif
}

Vec2D TextInput::measureLayout(float width,
                               LayoutMeasureMode widthMode,
                               float height,
                               LayoutMeasureMode heightMode)
{
#ifdef WITH_RIVE_TEXT
    AABB bounds =
        m_rawTextInput.measure(widthMode == LayoutMeasureMode::undefined
                                   ? std::numeric_limits<float>::max()
                                   : width,
                               heightMode == LayoutMeasureMode::undefined
                                   ? std::numeric_limits<float>::max()
                                   : height);
    return bounds.size();
#else
    return Vec2D();
#endif
}

void TextInput::controlSize(Vec2D size,
                            LayoutScaleType widthScaleType,
                            LayoutScaleType heightScaleType,
                            LayoutDirection direction)
{
    m_layoutWidth = size.x;
    updateMultiline();
}

std::string TextInput::strippedLineBreaks(const std::string& text)
{
    std::string stripped;
    stripped.reserve(text.size());
    bool inLineBreak = false;
    for (char c : text)
    {
        if (c == '\n' || c == '\r')
        {
            if (!inLineBreak)
            {
                stripped.push_back(' ');
                inLineBreak = true;
            }
        }
        else
        {
            stripped.push_back(c);
            inLineBreak = false;
        }
    }
    return stripped;
}

std::string TextInput::displayedText() const
{
    return multiline() ? m_sourceText : strippedLineBreaks(m_sourceText);
}

void TextInput::syncDisplayedTextFromSource(bool preserveCursor)
{
#ifdef WITH_RIVE_TEXT
    std::string nextDisplayText = displayedText();
    if (m_rawTextInput.text() == nextDisplayText)
    {
        return;
    }
    if (preserveCursor)
    {
        m_rawTextInput.textPreserveCursor(nextDisplayText);
    }
    else
    {
        m_rawTextInput.text(nextDisplayText);
    }
#endif
}

void TextInput::syncSourceTextFromRaw()
{
#ifdef WITH_RIVE_TEXT
    std::string rawText = m_rawTextInput.text();
    if (!multiline())
    {
        std::string singleLineText = strippedLineBreaks(rawText);
        if (singleLineText != rawText)
        {
            m_rawTextInput.textPreserveCursor(singleLineText);
            rawText = std::move(singleLineText);
        }
    }
    m_sourceText = std::move(rawText);
#endif
}

#ifdef WITH_RIVE_TEXT
static CursorBoundary cursorBoundary(KeyModifiers modifiers)
{
    if ((modifiers & KeyModifiers::meta) != KeyModifiers::none)
    {
        return CursorBoundary::line;
    }
    if ((modifiers & KeyModifiers::alt) != KeyModifiers::none)
    {
        if ((modifiers & KeyModifiers::ctrl) != KeyModifiers::none)
        {
            return CursorBoundary::subWord;
        }
        return CursorBoundary::word;
    }
    return CursorBoundary::character;
}

#if defined(__EMSCRIPTEN__)
EM_JS(bool, isWindowsBrowser, (), {
    return Boolean(navigator.platform.indexOf('Win') > -1);
});
#endif

static KeyModifiers systemModifier()
{
#if defined(__EMSCRIPTEN__)
    return isWindowsBrowser() ? KeyModifiers::ctrl : KeyModifiers::meta;
#elif defined(RIVE_WINDOWS)
    return KeyModifiers::ctrl;
#else
    return KeyModifiers::meta;
#endif
}

#endif

void TextInput::updateMultiline(bool syncDisplayedText)
{
#ifdef WITH_RIVE_TEXT
    if (multiline())
    {
        m_rawTextInput.maxWidth(m_layoutWidth);
        m_rawTextInput.sizing(TextSizing::autoHeight);
    }
    else
    {
        m_rawTextInput.maxWidth(0);
        m_rawTextInput.sizing(TextSizing::autoWidth);
    }

    if (m_scrollConstraint != nullptr)
    {
        if (multiline() && m_scrollConstraint->scrollOffsetX() != 0.0f)
        {
            m_scrollConstraint->stopPhysics();
            m_scrollConstraint->scrollOffsetX(0.0f);
        }
        else if (!multiline() && m_scrollConstraint->scrollOffsetY() != 0.0f)
        {
            m_scrollConstraint->stopPhysics();
            m_scrollConstraint->scrollOffsetY(0.0f);
        }
    }

    if (syncDisplayedText)
    {
        syncDisplayedTextFromSource(true);
    }

#ifdef WITH_RIVE_LAYOUT
    markLayoutNodeDirty();
#endif
    addDirt(ComponentDirt::TextShape);
#endif
}

bool TextInput::keyInput(Key value,
                         KeyModifiers modifiers,
                         bool isPressed,
                         bool isRepeat)
{
#ifdef WITH_RIVE_TEXT
    if (isPressed)
    {
        switch (value)
        {
            case Key::z:

                if ((modifiers & (systemModifier() | KeyModifiers::shift)) ==
                    (systemModifier() | KeyModifiers::shift))
                {
                    m_rawTextInput.redo();
                    syncSourceTextFromRaw();
                    markShapeDirty();
                    return true;
                }
                else if ((modifiers & systemModifier()) != KeyModifiers::none)
                {
                    m_rawTextInput.undo();
                    syncSourceTextFromRaw();
                    markShapeDirty();
                    return true;
                }
                break;
            case Key::backspace:
                m_rawTextInput.backspace(-1);
                syncSourceTextFromRaw();
                markShapeDirty();
                return true;
            case Key::deleteKey:
                m_rawTextInput.backspace(1);
                syncSourceTextFromRaw();
                markShapeDirty();
                return true;
            case Key::left:
                m_rawTextInput.cursorLeft(cursorBoundary(modifiers),
                                          (modifiers & KeyModifiers::shift) !=
                                              KeyModifiers::none);
                markPaintDirty();
                return true;
            case Key::right:
                m_rawTextInput.cursorRight(cursorBoundary(modifiers),
                                           (modifiers & KeyModifiers::shift) !=
                                               KeyModifiers::none);
                markPaintDirty();
                return true;
            case Key::up:
                m_rawTextInput.cursorUp((modifiers & KeyModifiers::shift) !=
                                        KeyModifiers::none);
                markPaintDirty();
                return true;
            case Key::down:
                m_rawTextInput.cursorDown((modifiers & KeyModifiers::shift) !=
                                          KeyModifiers::none);
                markPaintDirty();
                return true;
            default:
                return false;
        }
    }
#endif
    return false;
}

bool TextInput::textInput(const std::string& text)
{
#ifdef WITH_RIVE_TEXT
    std::string textToInsert = multiline() ? text : strippedLineBreaks(text);
    if (textToInsert.empty())
    {
        return true;
    }
    m_rawTextInput.insert(textToInsert);
    syncSourceTextFromRaw();
    markShapeDirty();
#endif
    return true;
}

void TextInput::focused()
{
    // TODO: Implement focus visual feedback
}

void TextInput::blurred()
{
    // TODO: Implement blur visual feedback
}

bool TextInput::worldPosition(Vec2D& outPosition)
{
    // TextInput is a TransformComponent, so we can access worldTransform
    // directly
    Vec2D localPos = worldTranslation();

    // Transform to root artboard space (handles nested artboards)
    auto* ab = artboard();
    if (ab)
    {
        outPosition = ab->rootTransform(localPos);
    }
    else
    {
        outPosition = localPos;
    }
    return true;
}

bool TextInput::worldBounds(AABB& outBounds)
{
    // m_worldBounds is computed in update() via
    // worldTransform().mapBoundingBox()
    if (m_worldBounds.isEmptyOrNaN())
    {
        return false;
    }

    // Transform to root artboard space (handles nested artboards)
    auto* ab = artboard();
    if (ab)
    {
        Vec2D minPt = ab->rootTransform(m_worldBounds.min());
        Vec2D maxPt = ab->rootTransform(m_worldBounds.max());
        outBounds = AABB(minPt, maxPt);
    }
    else
    {
        outBounds = m_worldBounds;
    }
    return true;
}

float TextInput::edgeScrollSpeedForDistance(float distanceFromEdge) const
{
    static constexpr float kEdgeScrollBaseSpeed = 45.0f;
    static constexpr float kEdgeScrollMaxSpeed = 400.0f;
    static constexpr float kEdgeScrollSpeedRamp = 4.0f;
    float speed =
        kEdgeScrollBaseSpeed + distanceFromEdge * kEdgeScrollSpeedRamp;
    return std::max(kEdgeScrollBaseSpeed, std::min(kEdgeScrollMaxSpeed, speed));
}

float TextInput::edgeActivationDistance(float position, float edgeStart) const
{
    return position >= edgeStart ? 0.0f : edgeStart - position;
}

bool TextInput::worldToLocalWithViewport(Vec2D worldPosition,
                                         Vec2D& outLocal,
                                         bool enableAutoScroll)
{
    m_scrollX = 0.0f;
    m_scrollY = 0.0f;

    // Get the inverse of the world transform to convert to local coordinates
    Mat2D inverseWorld;
    if (!worldTransform().invert(&inverseWorld))
    {
        return false;
    }

    Vec2D localPos = inverseWorld * worldPosition;

    // If we have a scroll constraint, handle viewport clamping and edge
    // detection.
    if (m_scrollConstraint != nullptr)
    {
        float viewportWidth = m_scrollConstraint->viewportWidth();
        float viewportHeight = m_scrollConstraint->viewportHeight();
        float scrollOffsetX = m_scrollConstraint->scrollOffsetX();
        float scrollOffsetY = m_scrollConstraint->scrollOffsetY();
        const float edgeThreshold = 20.0f;
        bool useHorizontalScroll =
            !multiline() && m_scrollConstraint->constrainsHorizontal();
        bool useVerticalScroll =
            multiline() && m_scrollConstraint->constrainsVertical();

        if (useHorizontalScroll)
        {
            // Convert to viewport-relative coordinates.
            float viewportX = localPos.x + scrollOffsetX;
            float leftDistance =
                edgeActivationDistance(viewportX, edgeThreshold);
            float rightDistance =
                edgeActivationDistance(viewportWidth - viewportX,
                                       edgeThreshold);

            // Edge detection for auto-scrolling.
            if (enableAutoScroll && leftDistance > 0.0f)
            {
                m_scrollX = edgeScrollSpeedForDistance(leftDistance);
                if (viewportX < 0.0f)
                {
                    localPos.x = -scrollOffsetX;
                }
            }
            else if (enableAutoScroll && rightDistance > 0.0f)
            {
                m_scrollX = -edgeScrollSpeedForDistance(rightDistance);
                if (viewportX > viewportWidth)
                {
                    localPos.x = viewportWidth - scrollOffsetX;
                }
            }
        }

        if (useVerticalScroll)
        {
            // Convert to viewport-relative coordinates.
            float viewportY = localPos.y + scrollOffsetY;
            float topDistance =
                edgeActivationDistance(viewportY, edgeThreshold);
            float bottomDistance =
                edgeActivationDistance(viewportHeight - viewportY,
                                       edgeThreshold);

            // Edge detection for auto-scrolling.
            if (enableAutoScroll && topDistance > 0.0f)
            {
                m_scrollY = edgeScrollSpeedForDistance(topDistance);
                if (viewportY < 0.0f)
                {
                    localPos.y = -scrollOffsetY;
                }
            }
            else if (enableAutoScroll && bottomDistance > 0.0f)
            {
                m_scrollY = -edgeScrollSpeedForDistance(bottomDistance);
                if (viewportY > viewportHeight)
                {
                    localPos.y = viewportHeight - scrollOffsetY;
                }
            }
        }
    }

    outLocal = localPos;
    return true;
}

void TextInput::startDrag(Vec2D worldPosition)
{
#ifdef WITH_RIVE_TEXT
    m_isDragging = true;
    m_lastDragWorldPosition = worldPosition;

    Vec2D localPos;
    if (!worldToLocalWithViewport(worldPosition, localPos, false))
    {
        return;
    }

    m_rawTextInput.moveCursorTo(localPos, false);
    markPaintDirty();
    // Note: Focus is handled by TextInputListenerGroup which can cross
    // artboard boundaries to find the appropriate FocusData.
#endif
}

void TextInput::drag(Vec2D worldPosition)
{
#ifdef WITH_RIVE_TEXT
    m_lastDragWorldPosition = worldPosition;

    Vec2D localPos;
    if (!worldToLocalWithViewport(worldPosition, localPos, true))
    {
        return;
    }

    m_rawTextInput.moveCursorTo(localPos, true);
    markPaintDirty();
#endif
}

void TextInput::endDrag(Vec2D worldPosition)
{
    m_isDragging = false;
    m_lastDragWorldPosition = Vec2D(NAN, NAN);
    m_scrollX = 0.0f;
    m_scrollY = 0.0f;
}

bool TextInput::advanceDrag(float elapsedSeconds)
{
#ifdef WITH_RIVE_TEXT
    if (!m_isDragging)
    {
        m_scrollX = 0.0f;
        m_scrollY = 0.0f;
        return false;
    }

    if ((m_scrollX == 0.0f && m_scrollY == 0.0f) ||
        m_scrollConstraint == nullptr)
    {
        return false;
    }

    m_scrollConstraint->stopPhysics();
    if (m_scrollX != 0.0f)
    {
        float scrollDeltaX = m_scrollX * elapsedSeconds;
        float newScrollOffsetX =
            m_scrollConstraint->scrollOffsetX() + scrollDeltaX;
        if (!m_scrollConstraint->infinite())
        {
            newScrollOffsetX = std::max(m_scrollConstraint->maxOffsetX(),
                                        std::min(0.0f, newScrollOffsetX));
        }
        m_scrollConstraint->scrollOffsetX(newScrollOffsetX);
    }
    if (m_scrollY != 0.0f)
    {
        float scrollDeltaY = m_scrollY * elapsedSeconds;
        float newScrollOffsetY =
            m_scrollConstraint->scrollOffsetY() + scrollDeltaY;
        if (!m_scrollConstraint->infinite())
        {
            newScrollOffsetY = std::max(m_scrollConstraint->maxOffsetY(),
                                        std::min(0.0f, newScrollOffsetY));
        }
        m_scrollConstraint->scrollOffsetY(newScrollOffsetY);
    }

    // Keep selection/caret in sync while edge-scrolling even if pointer isn't
    // moving.
    if (std::isfinite(m_lastDragWorldPosition.x) &&
        std::isfinite(m_lastDragWorldPosition.y))
    {
        Vec2D localPos;
        if (worldToLocalWithViewport(m_lastDragWorldPosition, localPos, true))
        {
            m_rawTextInput.moveCursorTo(localPos, true);
            markPaintDirty();
        }
    }

    // Continue scrolling while still dragging
    return m_isDragging;
#else
    return false;
#endif
}

bool TextInput::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
    return advanceDrag(elapsedSeconds);
}
