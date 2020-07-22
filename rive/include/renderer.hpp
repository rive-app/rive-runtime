#ifndef _RIVE_RENDERER_HPP_
#define _RIVE_RENDERER_HPP_
namespace rive
{
	class Mat2D;
	class Vec2D;
	/// Abstract path used implemented by the renderer.
	class RenderPath
	{
	public:
		virtual ~RenderPath() {}
		virtual void reset() = 0;
		// TODO: add commands like cubicTo, moveTo, etc...
		virtual void addPath(RenderPath* path, Mat2D* transform) = 0;
	};

	// struct RenderColorStop
	// {
	// 	unsigned int color;
	// 	float stop;
	// };

	class RenderPaint
	{
	public:
		virtual void color(unsigned int value) = 0;
		// virtual void linearGradient(Vec2D* start, Vec2D* end, RenderColorStop* stops, int stopsLength) = 0;
		// virtual void radialGradient(Vec2D* start, Vec2D* end, RenderColorStop* stops, int stopsLength) = 0;
		virtual ~RenderPaint() {}
	};

	extern RenderPath* makeRenderPath();
	extern RenderPaint* makeRenderPaint();
} // namespace rive
#endif