#ifndef _RIVE_METRICS_PATH_HPP_
#define _RIVE_METRICS_PATH_HPP_

#include "rive/command_path.hpp"
#include "rive/math/vec2d.hpp"
#include <cassert>
#include <vector>

namespace rive
{

	struct CubicSegment
	{
		float t;
		float length;
		CubicSegment(float tValue, float lengthValue) :
		    t(tValue), length(lengthValue)
		{
		}
	};

	struct PathPart
	{
		static const unsigned char line = 0;
		/// Type is 0 when this is a line segment, it's 1 or greater when it's a
		/// cubic. When it's a cubic it also represents the index in
		/// CubicSegments-1.
		unsigned char type;

		/// Offset is the offset in original path points (which get transformed
		/// when they're added to another path).
		unsigned char offset;

		// Only used by the cubic to count the number of cubic segments used by
		// this part.
		unsigned char numSegments;

		PathPart(unsigned char t, unsigned char l) :
		    type(t), offset(l), numSegments(0)
		{
		}
	};

	class MetricsPath : public CommandPath
	{
	private:
		std::vector<Vec2D> m_Points;
		std::vector<Vec2D> m_TransformedPoints;
		std::vector<CubicSegment> m_CubicSegments;
		std::vector<PathPart> m_Parts;
		std::vector<float> m_Lengths;
		std::vector<MetricsPath*> m_Paths;
		float m_ComputedLength = 0.0f;
		Mat2D m_ComputedLengthTransform;

	public:
		const std::vector<MetricsPath*>& paths() const { return m_Paths; }
		void addPath(CommandPath* path, const Mat2D& transform) override;
		void reset() override;
		void moveTo(float x, float y) override;
		void lineTo(float x, float y) override;
		void cubicTo(
		    float ox, float oy, float ix, float iy, float x, float y) override;
		void close() override;

		float length() const { return m_ComputedLength; }

		/// Add commands to the result RenderPath that will draw the segment
		/// from startLength to endLength of this MetricsPath. Requires
		/// computeLength be called prior to trimming.
		void trim(float startLength,
		          float endLength,
		          bool moveTo,
		          RenderPath* result);

	private:
		float computeLength(const Mat2D& transform);

		/// Extract a single segment from startT to endT as render commands
		/// added to result.
		void extractSubPart(int index,
		                    float startT,
		                    float endT,
		                    bool moveTo,
		                    RenderPath* result);
	};

	class OnlyMetricsPath : public MetricsPath
	{
	public:
		void fillRule(FillRule value) override {}

		RenderPath* renderPath() override
		{
			// Should never be used for actual rendering.
			assert(false);
			return nullptr;
		}
	};

	class RenderMetricsPath : public MetricsPath
	{
	private:
		RenderPath* m_RenderPath;

	public:
		RenderMetricsPath();
		~RenderMetricsPath();
		RenderPath* renderPath() override { return m_RenderPath; }
		void addPath(CommandPath* path, const Mat2D& transform) override;

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