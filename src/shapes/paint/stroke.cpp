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
#ifdef WITH_RIVE_EDITOR
        // Same hazard as LinearGradient::update — coop hydration order
        // can leave a Stroke in m_DependencyOrder with m_RenderPaint
        // null (mutator child arrived in a later batch, or all
        // initPaintMutator attempts returned InvalidObject because of
        // duplicate mutators in a malformed file). Pass 4-cull only
        // runs on the newly-hydrated batch, so a previously-orphaned
        // Stroke whose ancestor chain re-resolves later can re-enter
        // the live update set with m_RenderPaint still null.
        // Mirrors the Dart editor's late-init nullability — see the
        // long-form note in linear_gradient.cpp.
        if (m_RenderPaint == nullptr)
        {
            return;
        }
#else
        assert(m_RenderPaint != nullptr);
#endif
        m_RenderPaint->thickness(thickness());
        m_RenderPaint->cap((StrokeCap)cap());
        m_RenderPaint->join((StrokeJoin)join());
    }
}

void Stroke::invalidateRendering()
{
#ifdef WITH_RIVE_EDITOR
    if (m_RenderPaint == nullptr)
    {
        return;
    }
#else
    assert(m_RenderPaint != nullptr);
#endif
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