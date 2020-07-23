#ifndef _RIVE_NOOP_RENDERER_HPP_
#define _RIVE_NOOP_RENDERER_HPP_
#include "renderer.hpp"
#include <vector>

namespace rive
{
	class NoOpRenderPaint : public RenderPaint
	{
	public:
		void color(unsigned int value) {}
	};

	enum class NoOpPathCommandType
	{
		MoveTo,
		LineTo,
		CubicTo,
		Reset,
		Close
	};

	struct NoOpPathCommand
	{
		NoOpPathCommandType command;
		float x;
		float y;
		float inX;
		float inY;
		float outX;
		float outY;
	};

	class NoOpRenderPath : public RenderPath
	{
	public:
		std::vector<NoOpPathCommand> commands;
		void reset() override
		{
			commands.emplace_back((NoOpPathCommand){NoOpPathCommandType::Reset,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f});
		}
		void addPath(RenderPath* path, Mat2D* transform) override {}

		void moveTo(float x, float y) override
		{
			commands.emplace_back((NoOpPathCommand){
			    NoOpPathCommandType::MoveTo, x, y, 0.0f, 0.0f, 0.0f, 0.0f});
		}
		void lineTo(float x, float y) override
		{
			commands.emplace_back((NoOpPathCommand){
			    NoOpPathCommandType::LineTo, x, y, 0.0f, 0.0f, 0.0f, 0.0f});
		}
		void cubicTo(
		    float ox, float oy, float ix, float iy, float x, float y) override
		{
			commands.emplace_back((NoOpPathCommand){
			    NoOpPathCommandType::CubicTo, x, y, ix, iy, ox, oy});
		}
		void close() override
		{
			commands.emplace_back((NoOpPathCommand){NoOpPathCommandType::Close,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f,
			                                        0.0f});
		}
	};

} // namespace rive
#endif