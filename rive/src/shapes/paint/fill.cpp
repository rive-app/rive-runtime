#include "shapes/paint/fill.hpp"

using namespace rive;

PathSpace Fill::pathSpace() const { return PathSpace::Local; }

void Fill::draw(Renderer* renderer, RenderPath* path)
{
	renderer->drawPath(path, m_RenderPaint);
}