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
	addSegmentType(SegmentType::line);
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::addSegmentType(SegmentType type)
{
	m_SegmentTypes.push_back(SegmentInfo(type, m_Points.size()));
}

void MetricsPath::cubicTo(
    float ox, float oy, float ix, float iy, float x, float y)
{
	addSegmentType(SegmentType::cubic);
	m_Points.emplace_back(Vec2D(ox, oy));
	m_Points.emplace_back(Vec2D(ix, iy));
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::close()
{
	if (m_SegmentTypes.back().type == SegmentType::line)
	{
		// We auto close the last segment if it's a cubic, if it's not then make
		// sure to add the final segment in so we can compute its length too.
		addSegmentType(SegmentType::line);
		m_Points.emplace_back(m_Points[0]);
	}
}

float MetricsPath::computeLength()
{
	const Vec2D* pen = &m_Points[0];
	int idx = 1;
	float length = 0.0f;

	for (const SegmentInfo& segment : m_SegmentTypes)
	{
		switch (segment.type)
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

				// Compute cubic length by evaluating length of derivative
				// points for pre-computed T values. Known as Gauss quadrature:
				// https://pomax.github.io/bezierinfo/#arclength
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

					// sum length of the delta
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

void MetricsPath::trim(float startLength, float endLength, RenderPath* result)
{
	assert(endLength > startLength);

	// We need to find the first segment to trim.
	float length = 0.0f;

	int segmentCount = m_SegmentTypes.size();
	int firstSegmentIndex = -1, lastSegmentIndex = segmentCount - 1;
	float startT = 0.0f, endT = 1.0f;

	// Find first segment.
	for (int i = 0; i < segmentCount; i++)
	{
		float segmentLength = m_Lengths[i];
		if (length + segmentLength > startLength)
		{
			firstSegmentIndex = i;
			startT = (startLength - length) / segmentLength;
			break;
		}
		length += segmentLength;
	}
	// Find last segment.
	for (int i = firstSegmentIndex; i < segmentCount; i++)
	{
		float segmentLength = m_Lengths[i];
		if (length + segmentLength >= endLength)
		{
			lastSegmentIndex = i;
			endT = (endLength - length) / segmentLength;
			break;
		}
		length += segmentLength;
	}

	if (firstSegmentIndex == -1)
	{
		// Couldn't find it.
		return;
	}
	if (firstSegmentIndex == lastSegmentIndex)
	{
		extractSegment(firstSegmentIndex, startT, endT, true, result);
	}
	else
	{
		extractSegment(firstSegmentIndex, startT, 1.0f, true, result);
		for (int i = firstSegmentIndex + 1; i < lastSegmentIndex; i++)
		{
			// add entire segment...
			const SegmentInfo& segment = m_SegmentTypes[i];
			switch (segment.type)
			{
				case SegmentType::line:
				{
					const Vec2D& point = m_Points[segment.offset];
					result->lineTo(point[0], point[1]);
					break;
				}
				case SegmentType::cubic:
				{
					const Vec2D& point1 = m_Points[segment.offset];
					const Vec2D& point2 = m_Points[segment.offset + 1];
					const Vec2D& point3 = m_Points[segment.offset + 2];
					result->cubicTo(point1[0],
					                point1[1],
					                point2[0],
					                point2[1],
					                point3[0],
					                point3[1]);
					break;
				}
			}
		}
		extractSegment(lastSegmentIndex, 0.0f, endT, false, result);
	}
}

static void computeHull(const Vec2D& from,
                        const Vec2D& fromOut,
                        const Vec2D& toIn,
                        const Vec2D& to,
                        float t,
                        Vec2D* hull)
{
	Vec2D::lerp(hull[0], from, fromOut, t);
	Vec2D::lerp(hull[1], fromOut, toIn, t);
	Vec2D::lerp(hull[2], toIn, to, t);

	Vec2D::lerp(hull[3], hull[0], hull[1], t);
	Vec2D::lerp(hull[4], hull[1], hull[2], t);

	Vec2D::lerp(hull[5], hull[3], hull[4], t);
}

void MetricsPath::extractSegment(
    int index, float startT, float endT, bool moveTo, RenderPath* result)
{
	assert(startT >= 0.0f && startT <= 1.0f && endT >= 0.0f && endT <= 1.0f);

	const SegmentInfo& segment = m_SegmentTypes[index];
	switch (segment.type)
	{
		case SegmentType::line:
		{
			const Vec2D& from = m_Points[segment.offset - 1];
			const Vec2D& to = m_Points[segment.offset];
			Vec2D dir;
			Vec2D::subtract(dir, to, from);
			if (moveTo)
			{
				Vec2D point;
				Vec2D::scaleAndAdd(point, from, dir, startT);
				result->moveTo(point[0], point[1]);
			}
			Vec2D::scaleAndAdd(dir, from, dir, endT);
			result->lineTo(dir[0], dir[1]);

			break;
		}
		case SegmentType::cubic:
		{
			Vec2D hull[6];

			// TODO: cubic segment extraction.
			const Vec2D& from = m_Points[segment.offset - 1];
			const Vec2D& fromOut = m_Points[segment.offset];
			const Vec2D& toIn = m_Points[segment.offset + 1];
			const Vec2D& to = m_Points[segment.offset + 2];

			computeHull(from, fromOut, toIn, to, startT, hull);

			// left: from, hull[0], hull[3], hull[5]
			// right: hull[5], hull[4], hull[2], to
			break;
		}
	}
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