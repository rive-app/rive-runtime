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
		virtual void addPath(RenderPath* path, const Mat2D* transform) = 0;

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

		// void addRect(const AABB& bounds)
		// {
		// 	moveTo(bounds[0], bounds[1]);
		// 	lineTo(bounds[2], bounds[1]);
		// 	lineTo(bounds[2], bounds[3]);
		// 	lineTo(bounds[0], bounds[3]);
		// 	close();
		// }
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
		virtual void transform(const Mat2D* transform) = 0;
		virtual void translate(float x, float y) = 0;
		virtual void drawPath(RenderPath* path, RenderPaint* paint) = 0;
		virtual void clipPath(RenderPath* path) = 0;
	};

	extern RenderPath* makeRenderPath();
	extern RenderPaint* makeRenderPaint();
} // namespace rive
#endif