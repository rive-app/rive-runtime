#ifndef _RIVE_COMMAND_PATH_HPP_
#define _RIVE_COMMAND_PATH_HPP_

#include "rive/math/mat2d.hpp"

namespace rive
{
	enum class FillRule
	{
		nonZero,
		evenOdd
	};

	class RenderPath;

	/// Abstract path used to build up commands used for rendering.
	class CommandPath
	{
	public:
		virtual ~CommandPath() {}
		virtual void reset() = 0;
		virtual void fillRule(FillRule value) = 0;
		virtual void addPath(CommandPath* path, const Mat2D& transform) = 0;

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

		virtual RenderPath* renderPath() = 0;
	};
} // namespace rive
#endif