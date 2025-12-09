#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/profiler/profiler_macros.h"

using namespace rive;

StatusCode ShapePaint::onAddedClean(CoreContext* context)
{
    auto container = ShapePaintContainer::from(parent());
    if (container == nullptr)
    {
        return StatusCode::MissingObject;
    }

    // If the paint mutator wasn't compatible with this runtime it's possible we
    // get a ShapePaint with no mutator (not children).
    if (m_PaintMutator != nullptr)
    {
        container->addPaint(this);
    }

    return StatusCode::Ok;
}

void ShapePaint::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::Path) && m_effects.size() > 0)
    {
        auto container = ShapePaintContainer::from(parent());
        auto path = pickPath(container);
        for (auto& effect : m_effects)
        {
            effect->updateEffect(path, paintType());
            path = effect->effectPath();
        }
    }
}

RenderPaint* ShapePaint::initRenderPaint(ShapePaintMutator* mutator)
{
    assert(m_RenderPaint == nullptr);
    m_PaintMutator = mutator;

    auto factory = mutator->component()->artboard()->factory();
    m_RenderPaint = factory->makeRenderPaint();
    return m_RenderPaint.get();
}

void ShapePaint::blendMode(BlendMode parentValue)
{
    assert(m_RenderPaint != nullptr);
    // 127 means inherit
    if (blendModeValue() == 127)
    {
        m_RenderPaint->blendMode(parentValue);
    }
    else
    {
        m_RenderPaint->blendMode((BlendMode)blendModeValue());
    }
}

void ShapePaint::feather(Feather* feather) { m_feather = feather; }

Feather* ShapePaint::feather() const { return m_feather; }

void ShapePaint::draw(Renderer* renderer,
                      ShapePaintPath* shapePaintPath,
                      const Mat2D& transform,
                      bool usePathFillRule,
                      RenderPaint* overridePaint,
                      bool needsSaveOperation)
{
    RIVE_PROF_SCOPE()

    ShapePaintPath* pathToDraw = shapePaintPath;
    bool saved = !needsSaveOperation;
    if (m_feather != nullptr)
    {
        bool offsetInArtboard = m_feather->space() == TransformSpace::world;
        if (offsetInArtboard && !m_feather->inner())
        {
            if (m_feather->offsetX() != 0 || m_feather->offsetY() != 0)
            {
                if (!saved)
                {
                    saved = true;
                    renderer->save();
                }
                renderer->translate(m_feather->offsetX(), m_feather->offsetY());
            }
        }
    }
    if (shapePaintPath->isLocal())
    {
        if (!saved)
        {
            saved = true;
            renderer->save();
        }
        renderer->transform(transform);
    }

    if (m_effects.size() > 0)
    {
        pathToDraw = m_effects.back()->effectPath();
    }

    if (m_feather != nullptr)
    {
        if (m_feather->inner())
        {
            if (m_feather->innerPath() == nullptr)
            {
                return;
            }
            pathToDraw = m_feather->innerPath();
            if (!saved)
            {
                saved = true;
                renderer->save();
            }
            auto renderPath = shapePaintPath->renderPath(this);
            if (renderPath != nullptr)
            {
                renderer->clipPath(renderPath);
            }
        }

        // If we're offseting in world space, apply the offset last.
        if (m_feather->space() != TransformSpace::world &&
            !m_feather->inner() &&
            (m_feather->offsetX() != 0 || m_feather->offsetY() != 0))
        {
            if (!saved)
            {
                saved = true;
                renderer->save();
            }
            renderer->translate(m_feather->offsetX(), m_feather->offsetY());
        }
    }

    auto renderPath = pathToDraw->renderPath(this);
    if (renderPath != nullptr)
    {
        // Ugh, can't we make fillRule part of the Paint?
        if (!usePathFillRule && is<Fill>())
        {
            renderPath->fillRule((FillRule)as<Fill>()->fillRule());
        }

        renderer->drawPath(renderPath,
                           overridePaint != nullptr ? overridePaint
                                                    : renderPaint());
    }

    if (saved && needsSaveOperation)
    {
        renderer->restore();
    }
}

void ShapePaint::addStrokeEffect(StrokeEffect* effect)
{
    m_effects.push_back(effect);
}

void ShapePaint::invalidateEffects(StrokeEffect* invalidatingEffect)
{
    auto found = invalidatingEffect == nullptr;
    for (auto& effect : m_effects)
    {
        if (found)
        {
            effect->invalidateEffect();
        }
        if (invalidatingEffect && invalidatingEffect == effect)
        {
            found = true;
        }
    }
    invalidateRendering();
}

void ShapePaint::invalidateEffects() { invalidateEffects(nullptr); }

void ShapePaint::invalidateRendering() { addDirt(ComponentDirt::Path); }