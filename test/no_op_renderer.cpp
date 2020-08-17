#include "no_op_renderer.hpp"
#include "renderer.hpp"

namespace rive
{
	RenderPaint* makeRenderPaint() { return new NoOpRenderPaint(); }

	RenderPath* makeRenderPath() { return new NoOpRenderPath(); }
} // namespace rive