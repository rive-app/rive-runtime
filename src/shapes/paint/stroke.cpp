#include "rive/artboard.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
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

void Stroke::thicknessChanged() { addDirt(ComponentDirt::Paint); }

void Stroke::capChanged() { addDirt(ComponentDirt::Paint); }

void Stroke::joinChanged() { addDirt(ComponentDirt::Paint); }

void Stroke::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::Paint))
    {
        assert(m_RenderPaint != nullptr);
        m_RenderPaint->thickness(thickness());
        m_RenderPaint->cap((StrokeCap)cap());
        m_RenderPaint->join((StrokeJoin)join());
    }
}

void Stroke::invalidateRendering()
{
    assert(m_RenderPaint != nullptr);
    m_RenderPaint->invalidateStroke();
    Super::invalidateRendering();
}

ShapePaintPath* Stroke::pickPath(ShapePaintContainer* shape) const
{
    if (transformAffectsStroke())
    {
        return shape->localPath();
    }
    return shape->worldPath();
}

void Stroke::buildDependencies()
{
    auto container = ShapePaintContainer::from(parent());
    if (container != nullptr)
    {
        container->pathBuilder()->addDependent(this);
    }
}