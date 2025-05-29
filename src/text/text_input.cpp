#include "rive/text/text_input.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

void TextInput::draw(Renderer* renderer) {}

Core* TextInput::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

void TextInput::textChanged()
{
#ifdef WITH_RIVE_TEXT
    m_rawTextInput.text(m_Text);
#endif
}
void TextInput::selectionRadiusChanged() {}

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