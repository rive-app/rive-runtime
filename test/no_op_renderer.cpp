#include "no_op_renderer.hpp"
#include <rive/renderer.hpp>

namespace rive
{
	RenderPaint* makeRenderPaint() { return new NoOpRenderPaint(); }
	RenderPath* makeRenderPath() { return new NoOpRenderPath(); }
	RenderImage* makeRenderImage() { return new NoOpRenderImage(); }
} // namespace rive