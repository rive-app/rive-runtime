#include "shapes/metrics_path.hpp"
#include "bezier.hpp"

using namespace rive;

void MetricsPath::reset()
{
	m_Points.clear();
	m_SegmentTypes.clear();
	m_Lengths.clear();
	m_Paths.clear();
}

void MetricsPath::addPath(RenderPath* path, const Mat2D& transform)
{
	m_Paths.emplace_back(reinterpret_cast<MetricsPath*>(path));
}

void MetricsPath::moveTo(float x, float y)
{
	assert(m_Points.size() == 0);
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::lineTo(float x, float y)
{
	m_SegmentTypes.push_back(SegmentType::line);
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::cubicTo(
    float ox, float oy, float ix, float iy, float x, float y)
{
	m_SegmentTypes.push_back(SegmentType::cubic);
	m_Points.emplace_back(Vec2D(ox, oy));
	m_Points.emplace_back(Vec2D(ix, iy));
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::close()
{
	if (m_SegmentTypes.back() == SegmentType::line)
	{
		// We auto close the last segment if it's a cubic, if it's not then make
		// sure to add the final segment in so we can compute its length too.
		m_SegmentTypes.push_back(SegmentType::line);
		m_Points.emplace_back(m_Points[0]);
	}
}

float MetricsPath::computeLength()
{
	const Vec2D* pen = &m_Points[0];
	int idx = 1;
	float length = 0.0f;

	for (auto type : m_SegmentTypes)
	{
		switch (type)
		{
			case SegmentType::line:
			{
				const Vec2D& point = m_Points[idx++];
				float segmentLength = Vec2D::distance(*pen, point);
				m_Lengths.push_back(segmentLength);
				pen = &point;
				length += segmentLength;
				break;
			}
			case SegmentType::cubic:
			{
				// Compute the first derivative of the cubic (cubic->quadratic).
				// https://pomax.github.io/bezierinfo/#derivatives
				Vec2D derivativePoint1;
				Vec2D derivativePoint2;
				Vec2D derivativePoint3;
				Vec2D::subtract(derivativePoint1, pen[1], pen[0]);
				Vec2D::scale(derivativePoint1, derivativePoint1, 3.0f);

				Vec2D::subtract(derivativePoint2, pen[2], pen[1]);
				Vec2D::scale(derivativePoint2, derivativePoint2, 3.0f);

				Vec2D::subtract(derivativePoint3, pen[3], pen[2]);
				Vec2D::scale(derivativePoint3, derivativePoint3, 3.0f);

				float segmentLength = 0.0f;
				int steps = sizeof(Tvalues) / sizeof(Tvalues[0]);
				for (int i = 0; i < steps; i++)
				{
					float t = Tvalues[i];

					// Compute derivative x, y at t
					float it = 1.0f - t;
					float a = it * it;
					float b = it * t * 2.0f;
					float c = t * t;

					float x = a * derivativePoint1[0] +
					          b * derivativePoint2[0] + c * derivativePoint3[0];
					float y = a * derivativePoint1[1] +
					          b * derivativePoint2[1] + c * derivativePoint3[1];

					// length of the delta

					segmentLength += Cvalues[i] * sqrt(x * x + y * y);
				}

				segmentLength *= 0.5;
				m_Lengths.push_back(segmentLength);
				length += segmentLength;
				break;
			}
		}
	}
	return length;
}

void RenderMetricsPath::addPath(RenderPath* path, const Mat2D& transform)
{
	MetricsPath::addPath(path, transform);
	m_RenderPath->addPath(path, transform);
}

void RenderMetricsPath::reset()
{
	MetricsPath::reset();
	m_RenderPath->reset();
}

void RenderMetricsPath::moveTo(float x, float y)
{
	MetricsPath::moveTo(x, y);
	m_RenderPath->moveTo(x, y);
}

void RenderMetricsPath::lineTo(float x, float y)
{
	MetricsPath::lineTo(x, y);
	m_RenderPath->lineTo(x, y);
}

void RenderMetricsPath::cubicTo(
    float ox, float oy, float ix, float iy, float x, float y)
{
	MetricsPath::cubicTo(ox, oy, ix, iy, x, y);
	m_RenderPath->cubicTo(ox, oy, ix, iy, x, y);
}

void RenderMetricsPath::close()
{
	MetricsPath::close();
	m_RenderPath->close();
}

void RenderMetricsPath::fillRule(FillRule value)
{
	m_RenderPath->fillRule(value);
}