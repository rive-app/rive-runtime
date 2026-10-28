#include "rive/artboard.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_join.hpp"
#include "rive/shapes/paint/stroke_position.hpp"

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
  renderPaint->strokePosition(strokePosition());
  return renderPaint;
}

void Stroke::applyTo(RenderPaint* renderPaint, float opacityModifier)
{
  renderPaint->style(RenderPaintStyle::stroke);
  renderPaint->thickness(thickness());
  renderPaint->cap((StrokeCap)cap());
  renderPaint->join((StrokeJoin)join());
  renderPaint->strokePosition(strokePosition());
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

void Stroke::positionChanged() { addDirt(ComponentDirt::Paint); }

StrokePosition Stroke::strokePosition() const
{
  // An unknown value (e.g. from a newer file) draws centered, which is also
  // what renderers without stroke position support do.
  if (position() > (uint8_t)StrokePosition::outside)
  {
    return StrokePosition::center;
  }
  return (StrokePosition)position();
}

void Stroke::update(ComponentDirt value)
{
  Super::update(value);
  if (hasDirt(value, ComponentDirt::Paint))
  {
    // A paint with no color or gradient never gets a render paint; the
    // editor drops one, but a file can still carry it, as can coop hydration.
    if (m_RenderPaint == nullptr)
    {
      return;
    }
    m_RenderPaint->thickness(thickness());
    m_RenderPaint->cap((StrokeCap)cap());
    m_RenderPaint->join((StrokeJoin)join());
    m_RenderPaint->strokePosition(strokePosition());
  }
}

void Stroke::invalidateRendering()
{
  if (m_RenderPaint == nullptr)
  {
    return;
  }
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