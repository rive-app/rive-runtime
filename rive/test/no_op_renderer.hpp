#ifndef _RIVE_NOOP_RENDERER_HPP_
#define _RIVE_NOOP_RENDERER_HPP_
#include "renderer.hpp"

namespace rive
{
	class NoOpRenderPaint : public RenderPaint
	{
	public:
		void color(unsigned int value) {}
	};
	RenderPaint* makeRenderPaint() { return new NoOpRenderPaint(); }

	class NoOpRenderPath : public RenderPath
	{
	public:
		void reset() {}
		void addPath(RenderPath* path, Mat2D* transform) {}
	};
	RenderPath* makeRenderPath() { return new NoOpRenderPath(); }
} // namespace rive
#endif