#include "rive/text/text_input.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace rive;

void TextInput::draw(Renderer* renderer) {}

Core* TextInput::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

void TextInput::textChanged()
{
#ifdef WITH_RIVE_TEXT
    m_rawTextInput.text(m_Text);
#endif
}
void TextInput::selectionRadiusChanged()
{
#ifdef WITH_RIVE_TEXT
    m_rawTextInput.selectionCornerRadius(selectionRadius());
#endif
}

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

#ifdef WITH_RIVE_TEXT
    if (m_textStyle != nullptr && m_textStyle->font() != nullptr)
    {
        m_rawTextInput.font(m_textStyle->font());
    }
    m_rawTextInput.text(m_Text);
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
        if ((changed & RawTextInput::Flags::shapeDirty) != 0)
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
        if ((changed & RawTextInput::Flags::selectionDirty) != 0)
        {
            for (auto child : children<TextInputDrawable>())
            {
                child->invalidateStrokeEffects();
            }
        }
        if (m_scrollConstraint != nullptr)
        {
            CursorVisualPosition cursorPosition =
                m_rawTextInput.cursorVisualPosition();
            float viewportWidth = m_scrollConstraint->viewportWidth();
            float viewportHeight = m_scrollConstraint->viewportHeight();
            const float cursorWidth = 1.0f;

            float viewportX =
                cursorPosition.x() + m_scrollConstraint->scrollOffsetX();
            float viewportTop =
                cursorPosition.top() + m_scrollConstraint->scrollOffsetY();
            float viewportBottom =
                cursorPosition.bottom() + m_scrollConstraint->scrollOffsetY();

            if (viewportX < 0.0f)
            {
                m_scrollConstraint->stopPhysics();

                float scrollOffset =
                    m_scrollConstraint->scrollOffsetX() - viewportX;
                m_scrollConstraint->scrollOffsetX(scrollOffset);
            }
            else if (viewportX > viewportWidth - cursorWidth)
            {
                m_scrollConstraint->stopPhysics();

                float scrollOffset = m_scrollConstraint->scrollOffsetX() -
                                     (viewportX - viewportWidth + cursorWidth);
                m_scrollConstraint->scrollOffsetX(scrollOffset);
            }

            if (viewportTop < 0.0f)
            {
                m_scrollConstraint->stopPhysics();
                float scrollOffset =
                    m_scrollConstraint->scrollOffsetY() - viewportTop;
                m_scrollConstraint->scrollOffsetY(scrollOffset);
            }
            else if (viewportBottom > viewportHeight)
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
#ifdef WITH_RIVE_TEXT
    m_rawTextInput.maxWidth(size.x);
    m_rawTextInput.sizing(TextSizing::autoHeight);

    addDirt(ComponentDirt::TextShape);
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
                    markShapeDirty();
                    return true;
                }
                else if ((modifiers & systemModifier()) != KeyModifiers::none)
                {
                    m_rawTextInput.undo();
                    markShapeDirty();
                    return true;
                }
                break;
            case Key::backspace:
                m_rawTextInput.backspace(-1);
                markShapeDirty();
                return true;
            case Key::deleteKey:
                m_rawTextInput.backspace(1);
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
    m_rawTextInput.insert(text);
    markShapeDirty();
#endif
    return true;
}