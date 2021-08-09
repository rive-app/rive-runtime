#include "skia_renderer.hpp"
#include "SkGradientShader.h"
#include "SkPath.h"
#include "rive/math/vec2d.hpp"
#include "rive/shapes/paint/color.hpp"
#include "to_skia.hpp"

using namespace rive;

void SkiaRenderPath::fillRule(FillRule value)
{
	switch (value)
	{
		case FillRule::evenOdd:
			m_Path.setFillType(SkPathFillType::kEvenOdd);
			break;
		case FillRule::nonZero:
			m_Path.setFillType(SkPathFillType::kWinding);
			break;
	}
}

void SkiaRenderPath::reset() { m_Path.reset(); }
void SkiaRenderPath::addRenderPath(RenderPath* path, const Mat2D& transform)
{
	m_Path.addPath(reinterpret_cast<SkiaRenderPath*>(path)->m_Path,
	               ToSkia::convert(transform));
}

void SkiaRenderPath::moveTo(float x, float y) { m_Path.moveTo(x, y); }
void SkiaRenderPath::lineTo(float x, float y) { m_Path.lineTo(x, y); }
void SkiaRenderPath::cubicTo(
    float ox, float oy, float ix, float iy, float x, float y)
{
	m_Path.cubicTo(ox, oy, ix, iy, x, y);
}
void SkiaRenderPath::close() { m_Path.close(); }

SkiaRenderPaint::SkiaRenderPaint() { m_Paint.setAntiAlias(true); }

void SkiaRenderPaint::style(RenderPaintStyle style)
{
	switch (style)
	{
		case RenderPaintStyle::fill:
			m_Paint.setStyle(SkPaint::Style::kFill_Style);
			break;
		case RenderPaintStyle::stroke:
			m_Paint.setStyle(SkPaint::Style::kStroke_Style);
			break;
	}
}
void SkiaRenderPaint::color(unsigned int value) { m_Paint.setColor(value); }
void SkiaRenderPaint::thickness(float value) { m_Paint.setStrokeWidth(value); }
void SkiaRenderPaint::join(StrokeJoin value)
{
	m_Paint.setStrokeJoin(ToSkia::convert(value));
}
void SkiaRenderPaint::cap(StrokeCap value)
{
	m_Paint.setStrokeCap(ToSkia::convert(value));
}

void SkiaRenderPaint::linearGradient(float sx, float sy, float ex, float ey)
{
	m_GradientBuilder = new SkiaLinearGradientBuilder(sx, sy, ex, ey);
}
void SkiaRenderPaint::radialGradient(float sx, float sy, float ex, float ey)
{
	m_GradientBuilder = new SkiaRadialGradientBuilder(sx, sy, ex, ey);
}
void SkiaRenderPaint::addStop(unsigned int color, float stop)
{
	m_GradientBuilder->stops.emplace_back(GradientStop(color, stop));
}
void SkiaRenderPaint::completeGradient()
{
	m_GradientBuilder->make(m_Paint);
	delete m_GradientBuilder;
}

void SkiaRenderPaint::blendMode(BlendMode value)
{
	m_Paint.setBlendMode(ToSkia::convert(value));
}

void SkiaRadialGradientBuilder::make(SkPaint& paint)
{
	int numStops = stops.size();

	SkColor colors[numStops];
	SkScalar pos[numStops];

	for (int i = 0; i < numStops; i++)
	{
		const GradientStop& stop = stops[i];
		colors[i] = SkColor(stop.color);
		pos[i] = stop.stop;
	}

	float radius = Vec2D::distance(Vec2D(sx, sy), Vec2D(ex, ey));
	paint.setShader(SkGradientShader::MakeRadial(SkPoint::Make(sx, sy),
	                                             radius,
	                                             colors,
	                                             pos,
	                                             numStops,
	                                             SkTileMode::kClamp,
	                                             0,
	                                             nullptr));
}

void SkiaLinearGradientBuilder::make(SkPaint& paint)
{
	int numStops = stops.size();
	SkPoint points[2] = {SkPoint::Make(sx, sy), SkPoint::Make(ex, ey)};
	SkColor colors[numStops];
	SkScalar pos[numStops];

	for (int i = 0; i < numStops; i++)
	{
		const GradientStop& stop = stops[i];
		colors[i] = SkColor(stop.color);
		pos[i] = stop.stop;
	}
	paint.setShader(SkGradientShader::MakeLinear(
	    points, colors, pos, numStops, SkTileMode::kClamp, 0, nullptr));
}

void SkiaRenderer::save() { m_Canvas->save(); }
void SkiaRenderer::restore() { m_Canvas->restore(); }
void SkiaRenderer::transform(const Mat2D& transform)
{
	m_Canvas->concat(ToSkia::convert(transform));
}
void SkiaRenderer::drawPath(RenderPath* path, RenderPaint* paint)
{
	m_Canvas->drawPath(reinterpret_cast<SkiaRenderPath*>(path)->path(),
	                   reinterpret_cast<SkiaRenderPaint*>(paint)->paint());
}

void SkiaRenderer::clipPath(RenderPath* path)
{
	m_Canvas->clipPath(reinterpret_cast<SkiaRenderPath*>(path)->path(), true);
}

namespace rive
{
	RenderPath* makeRenderPath() { return new SkiaRenderPath(); }
	RenderPaint* makeRenderPaint() { return new SkiaRenderPaint(); }
} // namespace rive