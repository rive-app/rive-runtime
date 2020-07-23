#include "shapes/paint/stroke.hpp"

using namespace rive;

PathSpace Stroke::pathSpace() const { return transformAffectsStroke() ? PathSpace::Local : PathSpace::World; }


void Stroke::draw(Renderer* renderer, RenderPath* path)
{
	renderer->drawPath(path, m_RenderPaint);
}