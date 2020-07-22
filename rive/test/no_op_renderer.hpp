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
		void reset() override {}
		void addPath(RenderPath* path, Mat2D* transform) override {}

		void moveTo(float x, float y) override {}
		void lineTo(float x, float y) override {}
		void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override {}
		void close() override {}
	};
	RenderPath* makeRenderPath() { return new NoOpRenderPath(); }
} // namespace rive
#endif