#ifndef _RIVE_RENDERER_HPP_
#define _RIVE_RENDERER_HPP_

#include "layout.hpp"
#include "math/aabb.hpp"
#include "math/mat2d.hpp"
#include <cmath>
#include <stdio.h>

namespace rive
{
	class Vec2D;

	/// Abstract path used implemented by the renderer.
	class RenderPath
	{
	public:
		virtual ~RenderPath() {}
		virtual void reset() = 0;
		// TODO: add commands like cubicTo, moveTo, etc...
		virtual void addPath(RenderPath* path, const Mat2D& transform) = 0;

		virtual void moveTo(float x, float y) = 0;
		virtual void lineTo(float x, float y) = 0;
		virtual void
		cubicTo(float ox, float oy, float ix, float iy, float x, float y) = 0;
		virtual void close() = 0;

		void addRect(float x, float y, float width, float height)
		{
			moveTo(x, y);
			lineTo(x + width, y);
			lineTo(x + width, y + height);
			lineTo(x, y + height);
			close();
		}
	};

	enum class RenderPaintStyle
	{
		stroke,
		fill
	};

	class RenderPaint
	{
	public:
		virtual void style(RenderPaintStyle style) = 0;
		virtual void color(unsigned int value) = 0;
		virtual void thickness(float value) = 0;

		virtual void linearGradient(float sx, float sy, float ex, float ey) = 0;
		virtual void radialGradient(float sx, float sy, float ex, float ey) = 0;
		virtual void addStop(unsigned int color, float stop) = 0;
		virtual void completeGradient() = 0;
		// virtual void linearGradient(Vec2D* start, Vec2D* end,
		// RenderColorStop* stops, int stopsLength) = 0; virtual void
		// radialGradient(Vec2D* start, Vec2D* end, RenderColorStop* stops, int
		// stopsLength) = 0;
		virtual ~RenderPaint() {}
	};

	class Renderer
	{
	public:
		virtual ~Renderer() {}
		virtual void save() = 0;
		virtual void restore() = 0;
		virtual void transform(const Mat2D& transform) = 0;
		virtual void drawPath(RenderPath* path, RenderPaint* paint) = 0;
		virtual void clipPath(RenderPath* path) = 0;

		void align(Fit fit,
		           const Alignment& alignment,
		           const AABB& frame,
		           const AABB& content)
		{
			float contentWidth = content[2] - content[0];
			float contentHeight = content[3] - content[1];
			float x = -content[0] - contentWidth / 2.0 -
			          (alignment.x() * contentWidth / 2.0);
			float y = -content[1] - contentHeight / 2.0 -
			          (alignment.y() * contentHeight / 2.0);

			float scaleX = 1.0, scaleY = 1.0;

			switch (fit)
			{
				case Fit::fill: {
					scaleX = frame.width() / contentWidth;
					scaleY = frame.height() / contentHeight;
					break;
				}
				case Fit::contain: {
					float minScale = std::fmin(frame.width() / contentWidth,
					                           frame.height() / contentHeight);
					scaleX = scaleY = minScale;
					break;
				}
				case Fit::cover: {
					float maxScale = std::fmax(frame.width() / contentWidth,
					                           frame.height() / contentHeight);
					scaleX = scaleY = maxScale;
					break;
				}
				case Fit::fitHeight: {
					float minScale = frame.height() / contentHeight;
					scaleX = scaleY = minScale;
					break;
				}
				case Fit::fitWidth: {
					float minScale = frame.width() / contentWidth;
					scaleX = scaleY = minScale;
					break;
				}
				case Fit::none: {
					scaleX = scaleY = 1.0;
					break;
				}
				case Fit::scaleDown: {
					float minScale = std::fmin(frame.width() / contentWidth,
					                           frame.height() / contentHeight);
					scaleX = scaleY = minScale < 1.0 ? minScale : 1.0;
					break;
				}
			}

			Mat2D translation;
			translation[4] = frame[0] + frame.width() / 2.0 +
			                 (alignment.x() * frame.width() / 2.0);
			translation[5] = frame[1] + frame.height() / 2.0 +
			                 (alignment.y() * frame.height() / 2.0);
			Mat2D scale;
			scale[0] = scaleX;
			scale[3] = scaleY;

			Mat2D translateBack;
			translateBack[4] = x;
			translateBack[5] = y;

			Mat2D result;
			Mat2D::multiply(result, translation, scale);
			Mat2D::multiply(result, result, translateBack);
			transform(result);
		}
	};

	extern RenderPath* makeRenderPath();
	extern RenderPaint* makeRenderPaint();
} // namespace rive
#endif