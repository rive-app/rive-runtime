#include "shapes/metrics_path.hpp"
#include "renderer.hpp"
#include <math.h>

using namespace rive;

void MetricsPath::reset()
{
	m_ComputedLength = 0.0f;
	m_CubicSegments.clear();
	m_Points.clear();
	m_Parts.clear();
	m_Lengths.clear();
	m_Paths.clear();
}

void MetricsPath::addPath(CommandPath* path, const Mat2D& transform)
{
	MetricsPath* metricsPath = reinterpret_cast<MetricsPath*>(path);
	m_ComputedLength += metricsPath->computeLength(transform);
	m_Paths.emplace_back(metricsPath);
}

void MetricsPath::moveTo(float x, float y)
{
	assert(m_Points.size() == 0);
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::lineTo(float x, float y)
{
	m_Parts.push_back(PathPart(0, m_Points.size()));
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::cubicTo(
    float ox, float oy, float ix, float iy, float x, float y)
{
	m_Parts.push_back(PathPart(1, m_Points.size()));
	m_Points.emplace_back(Vec2D(ox, oy));
	m_Points.emplace_back(Vec2D(ix, iy));
	m_Points.emplace_back(Vec2D(x, y));
}

void MetricsPath::close()
{
	if (m_Parts.back().type == PathPart::line)
	{
		// We auto close the last part if it's a cubic, if it's not then make
		// sure to add the final part in so we can compute its length too.
		m_Parts.push_back(PathPart(0, m_Points.size()));
		m_Points.emplace_back(m_Points[0]);
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

static const float minSegmentLength = 0.05f;
static const float splitDistSquared = 0.5f;

static float distToSegmentSquared(const Vec2D& point,
                                  const Vec2D& p1,
                                  const Vec2D& diffToP2,
                                  float lengthSquared)
{
	Vec2D result;

	float t =
	    ((point[0] - p1[0]) * diffToP2[0] + (point[1] - p1[1]) * diffToP2[1]) /
	    lengthSquared;

	Vec2D::scaleAndAdd(result, p1, diffToP2, t);
	return Vec2D::distanceSquared(point, result);
}

static bool shouldSplitCubic(const Vec2D& from,
                             const Vec2D& fromOut,
                             const Vec2D& toIn,
                             const Vec2D& to)
{
	Vec2D diff;
	Vec2D::subtract(diff, from, to);
	float lengthSquared = Vec2D::lengthSquared(diff);
	if (lengthSquared < 0.0001f)
	{
		return Vec2D::distanceSquared(fromOut, from) > splitDistSquared ||
		       Vec2D::distanceSquared(toIn, from) > splitDistSquared;
	}

	return distToSegmentSquared(fromOut, from, diff, lengthSquared) >
	           splitDistSquared ||
	       distToSegmentSquared(toIn, from, diff, lengthSquared) >
	           splitDistSquared;
}

static float segmentCubic(const Vec2D& from,
                          const Vec2D& fromOut,
                          const Vec2D& toIn,
                          const Vec2D& to,
                          float runningLength,
                          float t1,
                          float t2,
                          std::vector<CubicSegment>& segments)
{
	if (shouldSplitCubic(from, fromOut, toIn, to))
	{
		float halfT = (t1 + t2) / 2.0f;

		Vec2D hull[6];
		computeHull(from, fromOut, toIn, to, 0.5f, hull);

		runningLength = segmentCubic(from,
		                             hull[0],
		                             hull[3],
		                             hull[5],
		                             runningLength,
		                             t1,
		                             halfT,
		                             segments);
		runningLength = segmentCubic(
		    hull[5], hull[4], hull[2], to, runningLength, halfT, t2, segments);
	}
	else
	{
		float length = Vec2D::distance(from, to);
		runningLength += length;
		if (length > minSegmentLength)
		{
			segments.emplace_back(CubicSegment(t2, runningLength));
		}
	}
	return runningLength;
}

float MetricsPath::computeLength(const Mat2D& transform)
{
	// Should never have subPaths with more subPaths (Skia allows this but for
	// Rive this isn't necessary and it keeps things simpler).
	assert(m_Paths.empty());
	const Vec2D* pen = &m_Points[0];
	int idx = 1;
	float length = 0.0f;

	// Transform all the points by transform. We know this works because we only
	// call computeLength once when this path is added as as subPath.
	for (Vec2D& point : m_Points)
	{
		Vec2D::transform(point, point, transform);
	}

	for (PathPart& part : m_Parts)
	{
		switch (part.type)
		{
			case PathPart::line:
			{
				const Vec2D& point = m_Points[idx++];

				float partLength = Vec2D::distance(*pen, point);
				m_Lengths.push_back(partLength);
				pen = &point;
				length += partLength;
				break;
			}
			// Anything above 0 is the number of cubic parts...
			default:
			{
				// Subdivide as necessary...

				// push in the parts

				const Vec2D& from = pen[0];
				const Vec2D& fromOut = pen[1];
				const Vec2D& toIn = pen[2];
				const Vec2D& to = pen[3];

				int index = m_CubicSegments.size();
				float partLength = segmentCubic(
				    from, fromOut, toIn, to, 0.0f, 0.0f, 1.0f, m_CubicSegments);
				m_Lengths.push_back(partLength);
				length += partLength;
				part.numSegments = m_CubicSegments.size() - index;
				break;
			}
		}
	}
	m_ComputedLength = length;
	return length;
}

void MetricsPath::trim(float startLength,
                       float endLength,
                       bool moveTo,
                       RenderPath* result)
{
	assert(endLength >= startLength);

	if (!m_Paths.empty())
	{
		m_Paths.front()->trim(startLength, endLength, moveTo, result);
		return;
	}
	// We need to find the first part to trim.
	float length = 0.0f;

	int partCount = m_Parts.size();
	int firstPartIndex = -1, lastPartIndex = partCount - 1;
	float startT = 0.0f, endT = 1.0f;

	// Find first part.
	for (int i = 0; i < partCount; i++)
	{
		float partLength = m_Lengths[i];
		if (length + partLength > startLength)
		{
			firstPartIndex = i;
			startT = (startLength - length) / partLength;
			break;
		}
		length += partLength;
	}
	if (firstPartIndex == -1)
	{
		// Couldn't find it.
		return;
	}

	// Find last part.
	for (int i = firstPartIndex; i < partCount; i++)
	{
		float partLength = m_Lengths[i];
		if (length + partLength >= endLength)
		{
			lastPartIndex = i;
			endT = (endLength - length) / partLength;
			break;
		}
		length += partLength;
	}

	if (firstPartIndex == lastPartIndex)
	{
		extractSubPart(firstPartIndex, startT, endT, moveTo, result);
	}
	else
	{
		extractSubPart(firstPartIndex, startT, 1.0f, moveTo, result);
		for (int i = firstPartIndex + 1; i < lastPartIndex; i++)
		{
			// add entire part...
			const PathPart& part = m_Parts[i];
			switch (part.type)
			{
				case PathPart::line:
				{
					const Vec2D& point = m_Points[part.offset];
					result->lineTo(point[0], point[1]);
					break;
				}
				default:
				{
					const Vec2D& point1 = m_Points[part.offset];
					const Vec2D& point2 = m_Points[part.offset + 1];
					const Vec2D& point3 = m_Points[part.offset + 2];
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
		extractSubPart(lastPartIndex, 0.0f, endT, false, result);
	}
}

float lerp(float from, float to, float f) { return from + f * (to - from); }

void MetricsPath::extractSubPart(
    int index, float startT, float endT, bool moveTo, RenderPath* result)
{
	assert(startT >= 0.0f && startT <= 1.0f && endT >= 0.0f && endT <= 1.0f);
	const PathPart& part = m_Parts[index];
	switch (part.type)
	{
		case PathPart::line:
		{
			const Vec2D& from = m_Points[part.offset - 1];
			const Vec2D& to = m_Points[part.offset];
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
		default:
		{
			auto startingSegmentIndex = part.type - 1;
			auto startEndSegmentIndex = startingSegmentIndex;
			auto endingSegmentIndex = startingSegmentIndex + part.numSegments;

			// Find cubicStartT and cubicEndT
			float length = m_Lengths[index];
			if (startT != 0.0f)
			{
				float startLength = startT * length;

				for (int si = startingSegmentIndex; si < endingSegmentIndex;
				     si++)
				{
					const CubicSegment& segment = m_CubicSegments[si];
					if (segment.length >= startLength)
					{
						if (si == startingSegmentIndex)
						{
							startT = segment.t * (startLength / segment.length);
						}
						else
						{
							float previousLength =
							    m_CubicSegments[si - 1].length;

							float t = (startLength - previousLength) /
							          (segment.length - previousLength);
							startT =
							    lerp(m_CubicSegments[si - 1].t, segment.t, t);
						}
						// Help out the ending segment finder by setting its
						// start to where we landed while finding the first
						// segment, that way it can skip a bunch of work.
						startEndSegmentIndex = si;
						break;
					}
				}
			}

			if (endT != 1.0f)
			{
				float endLength = endT * length;
				for (int si = startEndSegmentIndex; si < endingSegmentIndex;
				     si++)
				{
					const CubicSegment& segment = m_CubicSegments[si];
					if (segment.length >= endLength)
					{
						if (si == startingSegmentIndex)
						{
							endT = segment.t * (endLength / segment.length);
						}
						else
						{
							float previousLength =
							    m_CubicSegments[si - 1].length;

							float t = (endLength - previousLength) /
							          (segment.length - previousLength);
							endT =
							    lerp(m_CubicSegments[si - 1].t, segment.t, t);
						}
						break;
					}
				}
			}

			Vec2D hull[6];

			const Vec2D& from = m_Points[part.offset - 1];
			const Vec2D& fromOut = m_Points[part.offset];
			const Vec2D& toIn = m_Points[part.offset + 1];
			const Vec2D& to = m_Points[part.offset + 2];

			// printf("%f %f | %f %f | %f %f | %f %f\n",
			//        from[0],
			//        from[1],
			//        fromOut[0],
			//        fromOut[1],
			//        toIn[0],
			//        toIn[1],
			//        to[0],
			//        to[1]);
			// Computing the hull stores the left/right split as follows:
			// left: from, hull[0], hull[3], hull[5]
			// right: hull[5], hull[4], hull[2], to

			if (startT == 0.0f)
			{
				// Start is 0, so split at end and keep the left side.
				computeHull(from, fromOut, toIn, to, endT, hull);
				if (moveTo)
				{
					result->moveTo(from[0], from[1]);
				}
				result->cubicTo(hull[0][0],
				                hull[0][1],
				                hull[3][0],
				                hull[3][1],
				                hull[5][0],
				                hull[5][1]);
			}
			else
			{
				// Split at start since it's non 0.
				computeHull(from, fromOut, toIn, to, startT, hull);
				if (moveTo)
				{
					// Move to first point on the right side.
					result->moveTo(hull[5][0], hull[5][1]);
				}
				if (endT == 1.0f)
				{
					// End is 1, so no further split is necessary just cubicTo
					// the remaining right side.
					result->cubicTo(hull[4][0],
					                hull[4][1],
					                hull[2][0],
					                hull[2][1],
					                to[0],
					                to[1]);
				}
				else
				{
					// End is not 1, so split again and cubic to the left side
					// of the split and remap endT to the new curve range
					computeHull(hull[5],
					            hull[4],
					            hull[2],
					            to,
					            (endT - startT) / (1.0f - startT),
					            hull);

					result->cubicTo(hull[0][0],
					                hull[0][1],
					                hull[3][0],
					                hull[3][1],
					                hull[5][0],
					                hull[5][1]);
				}
			}
			break;
		}
	}
}

RenderMetricsPath::RenderMetricsPath() : m_RenderPath(makeRenderPath()) {}
RenderMetricsPath::~RenderMetricsPath() { delete m_RenderPath; }

void RenderMetricsPath::addPath(CommandPath* path, const Mat2D& transform)
{
	MetricsPath::addPath(path, transform);
	m_RenderPath->addPath(path->renderPath(), transform);
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