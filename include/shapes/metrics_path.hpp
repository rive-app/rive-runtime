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

	struct SegmentInfo
	{
		SegmentType type;
		unsigned char offset;
		SegmentInfo(SegmentType t, unsigned char l) : type(t), offset(l) {}
	};

	class MetricsPath : public RenderPath
	{
	private:
		std::vector<Vec2D> m_Points;
		std::vector<SegmentInfo> m_SegmentTypes;
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

		/// Add commands to the result RenderPath that will draw the segment
		/// from startLength to endLength of this MetricsPath. Requires
		/// computeLength be called prior to trimming.
		void trim(float startLength, float endLength, RenderPath* result);

	private:
		void addSegmentType(SegmentType type);
		/// Extract a single segment from startT to endT as render commands
		/// added to result.
		void
		extractSegment(int index, float startT, float endT, bool moveTo, RenderPath* result);
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