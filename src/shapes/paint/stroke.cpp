#include "rive/artboard.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/paint/stroke_join.hpp"

using namespace rive;

PathFlags Stroke::pathFlags() const
{
    return transformAffectsStroke() ? PathFlags::local : PathFlags::world;
}
RenderPaint* Stroke::initRenderPaint(ShapePaintMutator* mutator)
{
    auto renderPaint = Super::initRenderPaint(mutator);
    renderPaint->style(RenderPaintStyle::stroke);
    renderPaint->thickness(thickness());
    renderPaint->cap((StrokeCap)cap());
    renderPaint->join((StrokeJoin)join());
    return renderPaint;
}

void Stroke::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::Path) && m_effects.size() > 0)
    {
        auto container = ShapePaintContainer::from(parent());
        auto path = pickPath(container);
        for (auto& effect : m_effects)
        {
            effect->updateEffect(path);
            path = effect->effectPath();
        }
    }
}

void Stroke::applyTo(RenderPaint* renderPaint, float opacityModifier)
{
    renderPaint->style(RenderPaintStyle::stroke);
    renderPaint->thickness(thickness());
    renderPaint->cap((StrokeCap)cap());
    renderPaint->join((StrokeJoin)join());
    renderPaint->shader(nullptr);
    m_PaintMutator->applyTo(renderPaint, opacityModifier);
}

bool Stroke::isVisible() const
{
    return Super::isVisible() && thickness() > 0.0f;
}

void Stroke::thicknessChanged()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->thickness(thickness());
}

void Stroke::capChanged()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->cap((StrokeCap)cap());
}

void Stroke::joinChanged()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->join((StrokeJoin)join());
}

void Stroke::addStrokeEffect(StrokeEffect* effect)
{
    m_effects.push_back(effect);
}

void Stroke::invalidateEffects(StrokeEffect* invalidatingEffect)
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

void Stroke::invalidateEffects() { invalidateEffects(nullptr); }

void Stroke::invalidateRendering()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->invalidateStroke();
    addDirt(ComponentDirt::Path);
}

ShapePaintPath* Stroke::pickPath(ShapePaintContainer* shape) const
{
    if (transformAffectsStroke())
    {
        return shape->localPath();
    }
    return shape->worldPath();
}

void Stroke::draw(Renderer* renderer,
                  ShapePaintPath* shapePaintPath,
                  const Mat2D& transform,
                  bool usePathFillRule,
                  RenderPaint* overridePaint)
{
    if (m_effects.size() > 0)
    {
        shapePaintPath = m_effects.back()->effectPath();
    }

    Super::draw(renderer,
                shapePaintPath,
                transform,
                usePathFillRule,
                overridePaint);
}

void Stroke::buildDependencies()
{
    auto container = ShapePaintContainer::from(parent());
    if (container != nullptr)
    {
        container->pathBuilder()->addDependent(this);
    }
}