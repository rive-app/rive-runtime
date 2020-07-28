#include "shapes/path.hpp"
#include "math/circle_constant.hpp"
#include "renderer.hpp"
#include "shapes/cubic_vertex.hpp"
#include "shapes/path_vertex.hpp"
#include "shapes/shape.hpp"
#include "shapes/straight_vertex.hpp"
#include <cassert>

using namespace rive;

Path::~Path() { delete m_RenderPath; }

void Path::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	m_RenderPath = makeRenderPath();
}

void Path::onAddedClean(CoreContext* context)
{
	Super::onAddedClean(context);

	// Find the shape.
	for (auto currentParent = parent(); currentParent != nullptr;
	     currentParent = currentParent->parent())
	{
		if (currentParent->is<Shape>())
		{
			m_Shape = currentParent->as<Shape>();
			m_Shape->addPath(this);
			return;
		}
	}
}

void Path::addVertex(PathVertex* vertex) { m_Vertices.push_back(vertex); }

const Mat2D& Path::pathTransform() const
{
	// If we're bound to bounds, return identity as points will already be in
	// world space.
	return worldTransform();
}

static void buildPath(RenderPath& renderPath,
                      bool isClosed,
                      const std::vector<PathVertex*>& vertices)
{
	renderPath.reset();

	auto length = vertices.size();
	if (length < 2)
	{
		return;
	}
	auto firstPoint = vertices[0];

	// Init out to translation
	float outX, outY;
	bool prevIsCubic;

	float startX, startY;
	float startInX, startInY;
	bool startIsCubic;

	if (firstPoint->is<CubicVertex>())
	{
		auto cubic = firstPoint->as<CubicVertex>();
		startIsCubic = prevIsCubic = true;
		auto inPoint = cubic->inPoint();
		startInX = inPoint[0];
		startInY = inPoint[1];
		auto outPoint = cubic->outPoint();
		outX = outPoint[0];
		outY = outPoint[1];
		renderPath.moveTo(startX = cubic->x(), startY = cubic->y());
	}
	else
	{
		startIsCubic = prevIsCubic = false;
		auto point = *firstPoint->as<StraightVertex>();

		if (auto radius = point.radius(); radius > 0.0f)
		{
			auto prev = vertices[length - 1];

			Vec2D pos(point.x(), point.y());

			Vec2D toPrev;
			Vec2D::subtract(toPrev, Vec2D(prev->x(), prev->y()), pos);
			auto toPrevLength = Vec2D::length(toPrev);
			toPrev[0] /= toPrevLength;
			toPrev[1] /= toPrevLength;

			auto next = vertices[1];

			Vec2D toNext;
			Vec2D::subtract(toNext,
			                next->is<CubicVertex>()
			                    ? next->as<CubicVertex>()->inPoint()
			                    : Vec2D(next->x(), next->y()),
			                pos);
			auto toNextLength = Vec2D::length(toNext);
			toNext[0] /= toNextLength;
			toNext[1] /= toNextLength;

			float renderRadius =
			    std::min(toPrevLength, std::min(toNextLength, radius));

			Vec2D translation;
			Vec2D::scaleAndAdd(translation, pos, toPrev, renderRadius);
			renderPath.moveTo(startX = translation[0], startY = translation[1]);

			Vec2D outPoint;
			Vec2D::scaleAndAdd(
			    outPoint, pos, toPrev, icircleConstant * renderRadius);

			Vec2D inPoint;
			Vec2D::scaleAndAdd(
			    inPoint, pos, toNext, icircleConstant * renderRadius);

			Vec2D posNext;
			Vec2D::scaleAndAdd(posNext, pos, toNext, renderRadius);
			renderPath.cubicTo(outPoint[0],
			                   outPoint[1],
			                   inPoint[0],
			                   inPoint[1],
			                   startInX = outX = posNext[0],
			                   startInY = outY = posNext[1]);
			prevIsCubic = false;
		}
		else
		{
			outX = point.x();
			outY = point.y();
			renderPath.moveTo(startInX = startX = outX,
			                  startInY = startY = outY);
		}
	}

	for (int i = 1; i < length; i++)
	{
		auto vertex = vertices[i];

		if (vertex->is<CubicVertex>())
		{
			auto cubic = vertex->as<CubicVertex>();
			auto inPoint = cubic->inPoint();

			renderPath.cubicTo(
			    outX, outY, inPoint[0], inPoint[1], cubic->x(), cubic->y());

			prevIsCubic = true;
			auto outPoint = cubic->outPoint();
			outX = outPoint[0];
			outY = outPoint[1];
		}
		else
		{
			auto point = *vertex->as<StraightVertex>();

			if (auto radius = point.radius(); radius > 0.0f)
			{
				Vec2D pos(point.x(), point.y());

				Vec2D toPrev;
				Vec2D::subtract(toPrev, Vec2D(outX, outY), pos);
				auto toPrevLength = Vec2D::length(toPrev);
				toPrev[0] /= toPrevLength;
				toPrev[1] /= toPrevLength;

				auto next = vertices[(i + 1) % length];

				Vec2D toNext;
				Vec2D::subtract(toNext,
				                next->is<CubicVertex>()
				                    ? next->as<CubicVertex>()->inPoint()
				                    : Vec2D(next->x(), next->y()),
				                pos);
				auto toNextLength = Vec2D::length(toNext);
				toNext[0] /= toNextLength;
				toNext[1] /= toNextLength;

				float renderRadius =
				    std::min(toPrevLength, std::min(toNextLength, radius));

				Vec2D translation;
				Vec2D::scaleAndAdd(translation, pos, toPrev, renderRadius);
				if (prevIsCubic)
				{
					renderPath.cubicTo(outX,
					                   outY,
					                   translation[0],
					                   translation[1],
					                   translation[0],
					                   translation[1]);
				}
				else
				{
					renderPath.lineTo(translation[0], translation[1]);
				}

				Vec2D outPoint;
				Vec2D::scaleAndAdd(
				    outPoint, pos, toPrev, icircleConstant * renderRadius);

				Vec2D inPoint;
				Vec2D::scaleAndAdd(
				    inPoint, pos, toNext, icircleConstant * renderRadius);

				Vec2D posNext;
				Vec2D::scaleAndAdd(posNext, pos, toNext, renderRadius);
				renderPath.cubicTo(outPoint[0],
				                   outPoint[1],
				                   inPoint[0],
				                   inPoint[1],
				                   outX = posNext[0],
				                   outY = posNext[1]);
				prevIsCubic = false;
			}
			else if (prevIsCubic)
			{
				float x = point.x();
				float y = point.y();
				renderPath.cubicTo(outX, outY, x, y, x, y);

				prevIsCubic = false;
				outX = x;
				outY = y;
			}
			else
			{
				renderPath.lineTo(outX = point.x(), outY = point.y());
			}
		}
	}
	if (isClosed)
	{
		if (prevIsCubic || startIsCubic)
		{
			renderPath.cubicTo(outX, outY, startInX, startInY, startX, startY);
		}
		renderPath.close();
	}
}

void Path::markPathDirty()
{
	addDirt(ComponentDirt::Path);
	if (m_Shape != nullptr)
	{
		m_Shape->pathChanged();
	}
}

void Path::update(ComponentDirt value)
{
	Super::update(value);
	assert(m_RenderPath != nullptr);
	if (hasDirt(value, ComponentDirt::Path))
	{
		buildPath(*m_RenderPath, isPathClosed(), m_Vertices);
	}
}