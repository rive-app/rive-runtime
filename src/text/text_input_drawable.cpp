#include "rive/text/text_input_drawable.hpp"
#include "rive/text/text_input.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

using namespace rive;

TextInput* TextInputDrawable::textInput() const
{
    return parent()->as<TextInput>();
}

ShapePaintPath* TextInputDrawable::worldPath()
{
    RIVE_UNREACHABLE();
    return nullptr;
}

StatusCode TextInputDrawable::onAddedClean(CoreContext* context)
{
    if (!parent()->is<TextInput>())
    {
        return StatusCode::InvalidObject;
    }

    return StatusCode::Ok;
}

const Mat2D& TextInputDrawable::shapeWorldTransform() const
{
    return worldTransform();
}

void TextInputDrawable::draw(Renderer* renderer)
{
    if (renderOpacity() == 0.0f)
    {
        return;
    }
    ClipResult clipResult = applyClip(renderer);

    if (clipResult != ClipResult::emptyClip)
    {
        for (auto shapePaint : m_ShapePaints)
        {
            if (!shapePaint->isVisible())
            {
                continue;
            }
            auto shapePaintPath = shapePaint->pickPath(this);
            if (shapePaintPath == nullptr)
            {
                continue;
            }
            shapePaint->draw(renderer,
                             shapePaintPath,
                             textInput()->worldTransform());
        }
    }

    if (clipResult != ClipResult::noClip)
    {
        renderer->restore();
    }
}