#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/artboard.hpp"
#include "rive/transform_component.hpp"
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

#ifdef WITH_RIVE_EDITOR
    // Edit-time: Stroke / Fill::update derefs `m_RenderPaint` which
    // is set by `initRenderPaint` only if a child mutator
    // (SolidColor / LinearGradient) successfully initialized. Coop
    // can deliver duplicate or conflicting mutators where all but
    // the first return InvalidObject from `initPaintMutator`,
    // leaving `m_RenderPaint` null. Signal the dispatcher to cull
    // this ShapePaint so it doesn't enter `m_DependencyOrder` and
    // crash inside `update()`.
    if (m_RenderPaint == nullptr)
    {
        return StatusCode::InvalidObject;
    }
#endif

    return StatusCode::Ok;
}

void ShapePaint::update(ComponentDirt value)
{
    Super::update(value);
    auto shapeEffects = effects();
    if (hasDirt(value, ComponentDirt::Path) && shapeEffects->size() > 0)
    {
        auto container = ShapePaintContainer::from(parent());
        // Hidden paints are re-invalidated when shown, so measuring now is
        // wasted, unless a clip still reads the result.
        if (renderOpacity() == 0 &&
            (container->pathFlags() & PathFlags::clipping) == PathFlags::none)
        {
            return;
        }
        auto path = pickPath(container);
        for (auto& effect : *shapeEffects)
        {
            effect->updateEffect(this, path, this);
            auto newPath = effect->effectPath(this);
            if (newPath)
            {
                path = newPath;
            }
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

void ShapePaint::blendMode(BlendMode parentValue, uint8_t parentAdditiveAmount)
{
    assert(m_RenderPaint != nullptr);
    // 127 means inherit
    const bool inherits = blendModeValue() == 127;
    const BlendMode mode = inherits ? parentValue : (BlendMode)blendModeValue();
    const uint8_t amount = inherits ? parentAdditiveAmount : additiveAmount();

    m_RenderPaint->blendMode(mode);
    // Only additive is parameterized; every other mode ignores additiveness
    // anyway, but keep it at 0 so a stale value can never leak through.
    m_RenderPaint->additiveness(additivenessFor(mode, amount));
}

void ShapePaint::additiveAmountChanged()
{
    // additiveAmount animates and data binds, so it can change long after
    // buildDependencies() did the initial sync. A paint set to inherit takes
    // its amount from the parent drawable, which pushes it down itself.
    if (m_RenderPaint == nullptr || inheritsBlendMode())
    {
        return;
    }
    m_RenderPaint->blendMode(blendMode());
    m_RenderPaint->additiveness(additivenessFor(blendMode(), additiveAmount()));
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
    RIVE_PROF_SCOPE_L(2)

    ShapePaintPath* pathToDraw = shapePaintPath;
    bool saved = !needsSaveOperation;
    if (m_feather != nullptr)
    {
        bool offsetInArtboard = m_feather->space() == TransformSpace::world;
        if (offsetInArtboard && !m_feather->isInner())
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

    auto pathEffect = lastEffectPath(this);
    if (pathEffect)
    {
        pathToDraw = pathEffect;
    }

    if (m_feather != nullptr)
    {
        if (m_feather->isInner())
        {
            if (m_feather->innerPath() == nullptr)
            {
                // Bail out, but never leave the renderer's state stack
                // unbalanced: we may already have saved above.
                if (saved && needsSaveOperation)
                {
                    renderer->restore();
                }
                return;
            }
            // When a path effect is active, the inner path and clip must be
            // based on the effect-modified path, not the original shape path.
            // Only rebuild when the effect path has actually changed.
            if (pathEffect != nullptr && m_feather->effectPathDirty())
            {
                auto container = ShapePaintContainer::from(parent());
                if (container != nullptr)
                {
                    bool offsetInArtboard =
                        m_feather->space() == TransformSpace::world;
                    m_feather->rebuildInnerPath(
                        pathEffect,
                        container->shapeWorldTransform(),
                        offsetInArtboard);
                }
            }
            pathToDraw = m_feather->innerPath();
            if (!saved)
            {
                saved = true;
                renderer->save();
            }
            auto clipPath = pathEffect ? pathEffect : shapePaintPath;
            auto renderPath = clipPath->renderPath(this);
            if (renderPath != nullptr)
            {
                renderer->clipPath(renderPath);
            }
        }

        // If we're offseting in world space, apply the offset last.
        if (m_feather->space() != TransformSpace::world &&
            !m_feather->isInner() &&
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

        // Modulate this paint with the (optional) PaintImage child, fit to the
        // shape's local bounds. Skipped when drawing with an external override
        // paint. The child is discovered by type rather than cached.
        if (overridePaint == nullptr)
        {
            applyModulatedImage(shapePaintPath->rawPath()->bounds());
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

void ShapePaint::applyModulatedImage(const AABB& bounds)
{
    PaintImage* image = firstChild<PaintImage>();
    if (image != nullptr && image->applyTo(renderPaint(), bounds))
    {
        m_hasModulatedImage = true;
        return;
    }
    if (m_hasModulatedImage)
    {
        // No image to modulate with any more -- the child was removed, its
        // asset cleared, or the replacement hasn't decoded yet. The RenderPaint
        // persists across draws, so drop the stale texture explicitly.
        renderPaint()->modulatedImage(nullptr,
                                      ImageSampler::LinearClamp(),
                                      Mat2D());
        m_hasModulatedImage = false;
    }
}

void ShapePaint::invalidateEffects(StrokeEffect* invalidatingEffect)
{
    EffectsContainer::invalidateEffects(invalidatingEffect);
    if (m_feather != nullptr)
    {
        m_feather->markEffectPathDirty();
        // The path we paint changed; an inner feather derives its geometry
        // from that path so it has to rebuild.
        if (m_feather->isInner())
        {
            m_feather->addDirt(ComponentDirt::Path);
        }
    }
    invalidateRendering();
}

void ShapePaint::invalidateEffects() { invalidateEffects(nullptr); }

void ShapePaint::invalidateRendering() { addDirt(ComponentDirt::Path, true); }

void ShapePaint::addStrokeEffect(StrokeEffect* effect)
{
    effect->addPathProvider(this);
    EffectsContainer::addStrokeEffect(effect);
}

TransformComponent* ShapePaint::parentTransformComponent() const
{
    auto _parent = parent();
    while (_parent)
    {
        if (_parent->is<TransformComponent>())
        {
            return _parent->as<TransformComponent>();
        }
        _parent = _parent->parent();
    }
    return nullptr;
}