#ifndef _RIVE_METRICS_PATH_HPP_
#define _RIVE_METRICS_PATH_HPP_

#include "math/vec2d.hpp"
#include "renderer.hpp"
#include <vector>

namespace rive
{
	enum class SegmentType : unsigned char
	{
		line,
		cubic
	};

	class MetricsPath : public RenderPath
	{
	private:
		std::vector<Vec2D> m_Points;
		std::vector<SegmentType> m_SegmentTypes;
		std::vector<float> m_Lengths;
		std::vector<MetricsPath*> m_Paths;

	public:
		void addPath(RenderPath* path, const Mat2D& transform) override;
		void reset() override;
		void moveTo(float x, float y) override;
		void lineTo(float x, float y) override;
		void cubicTo(
		    float ox, float oy, float ix, float iy, float x, float y) override;
		void close() override;

		float computeLength();
	};

	class OnlyMetricsPath : public MetricsPath
	{
	public:
		void fillRule(FillRule value) override {}
	};

	class RenderMetricsPath : public MetricsPath
	{
	private:
		RenderPath* m_RenderPath;

	public:
		RenderMetricsPath() : m_RenderPath(makeRenderPath()) {}
		~RenderMetricsPath() { delete m_RenderPath; }
		RenderPath* renderPath() { return m_RenderPath; }
		void addPath(RenderPath* path, const Mat2D& transform) override;

		void fillRule(FillRule value) override;
		void reset() override;
		void moveTo(float x, float y) override;
		void lineTo(float x, float y) override;
		void cubicTo(
		    float ox, float oy, float ix, float iy, float x, float y) override;
		void close() override;
	};
} // namespace rive

#endif