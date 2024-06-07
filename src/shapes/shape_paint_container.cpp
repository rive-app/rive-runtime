#include "rive/shapes/shape_paint_container.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/component.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/text/text_style.hpp"

using namespace rive;

ShapePaintContainer* ShapePaintContainer::from(Component* component)
{
    switch (component->coreType())
    {
        case Artboard::typeKey:
            return component->as<Artboard>();
        case Shape::typeKey:
            return component->as<Shape>();
        case TextStyle::typeKey:
            return component->as<TextStyle>();
    }
    return nullptr;
}

void ShapePaintContainer::addPaint(ShapePaint* paint) { m_ShapePaints.push_back(paint); }

PathSpace ShapePaintContainer::pathSpace() const
{
    PathSpace space = m_DefaultPathSpace;
    for (auto paint : m_ShapePaints)
    {
        space |= paint->pathSpace();
    }
    return space;
}

void ShapePaintContainer::invalidateStrokeEffects()
{
    for (auto paint : m_ShapePaints)
    {
        if (paint->is<Stroke>())
        {
            paint->as<Stroke>()->invalidateEffects();
        }
    }
}

void ShapePaintContainer::propagateOpacity(float opacity)
{
    for (auto shapePaint : m_ShapePaints)
    {
        shapePaint->renderOpacity(opacity);
    }
}
