#include "shapes/path.hpp"
#include "math/circle_constant.hpp"
#include "renderer.hpp"
#include "shapes/cubic_vertex.hpp"
#include "shapes/path_vertex.hpp"
#include "shapes/shape.hpp"
#include "shapes/straight_vertex.hpp"
#include <cassert>

using namespace rive;

Path::~Path() { delete m_CommandPath; }

StatusCode Path::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	return StatusCode::Ok;
}

StatusCode Path::onAddedClean(CoreContext* context)
{
	StatusCode code = Super::onAddedClean(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}

	// Find the shape.
	for (auto currentParent = parent(); currentParent != nullptr;
	     currentParent = currentParent->parent())
	{
		if (currentParent->is<Shape>())
		{
			m_Shape = currentParent->as<Shape>();
			m_Shape->addPath(this);
			return StatusCode::Ok;
		}
	}

	return StatusCode::MissingObject;
}

void Path::buildDependencies()
{
	Super::buildDependencies();
	// Make sure this is called once the shape has all of the paints added
	// (paints get added during the added cycle so buildDependencies is a good
	// time to do this.)
	m_CommandPath = m_Shape->makeCommandPath(PathSpace::Neither);
}

void Path::addVertex(PathVertex* vertex) { m_Vertices.push_back(vertex); }

const Mat2D& Path::pathTransform() const { return worldTransform(); }

static void buildPath(CommandPath& commandPath,
                      bool isClosed,
                      const std::vector<PathVertex*>& vertices)
{
	commandPath.reset();

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
		auto inPoint = cubic->renderIn();
		startInX = inPoint[0];
		startInY = inPoint[1];
		auto outPoint = cubic->renderOut();
		outX = outPoint[0];
		outY = outPoint[1];
		auto translation = cubic->renderTranslation();
		commandPath.moveTo(startX = translation[0], startY = translation[1]);
	}
	else
	{
		startIsCubic = prevIsCubic = false;
		auto point = *firstPoint->as<StraightVertex>();

		if (auto radius = point.radius(); radius > 0.0f)
		{
			auto prev = vertices[length - 1];

			Vec2D pos = point.renderTranslation();

			Vec2D toPrev;
			Vec2D::subtract(toPrev,
			                prev->is<CubicVertex>()
			                    ? prev->as<CubicVertex>()->renderOut()
			                    : prev->renderTranslation(),
			                pos);
			auto toPrevLength = Vec2D::length(toPrev);
			toPrev[0] /= toPrevLength;
			toPrev[1] /= toPrevLength;

			auto next = vertices[1];

			Vec2D toNext;
			Vec2D::subtract(toNext,
			                next->is<CubicVertex>()
			                    ? next->as<CubicVertex>()->renderIn()
			                    : next->renderTranslation(),
			                pos);
			auto toNextLength = Vec2D::length(toNext);
			toNext[0] /= toNextLength;
			toNext[1] /= toNextLength;

			float renderRadius =
			    std::min(toPrevLength, std::min(toNextLength, radius));

			Vec2D translation;
			Vec2D::scaleAndAdd(translation, pos, toPrev, renderRadius);
			commandPath.moveTo(startInX = startX = translation[0],
			                   startInY = startY = translation[1]);
			Vec2D outPoint;
			Vec2D::scaleAndAdd(
			    outPoint, pos, toPrev, icircleConstant * renderRadius);

			Vec2D inPoint;
			Vec2D::scaleAndAdd(
			    inPoint, pos, toNext, icircleConstant * renderRadius);

			Vec2D posNext;
			Vec2D::scaleAndAdd(posNext, pos, toNext, renderRadius);
			commandPath.cubicTo(outPoint[0],
			                    outPoint[1],
			                    inPoint[0],
			                    inPoint[1],
			                    outX = posNext[0],
			                    outY = posNext[1]);
			prevIsCubic = false;
		}
		else
		{
			auto translation = point.renderTranslation();
			commandPath.moveTo(startInX = startX = outX = translation[0],
			                   startInY = startY = outY = translation[1]);
		}
	}

	for (size_t i = 1; i < length; i++)
	{
		auto vertex = vertices[i];

		if (vertex->is<CubicVertex>())
		{
			auto cubic = vertex->as<CubicVertex>();
			auto inPoint = cubic->renderIn();
			auto translation = cubic->renderTranslation();

			commandPath.cubicTo(outX,
			                    outY,
			                    inPoint[0],
			                    inPoint[1],
			                    translation[0],
			                    translation[1]);

			prevIsCubic = true;
			auto outPoint = cubic->renderOut();
			outX = outPoint[0];
			outY = outPoint[1];
		}
		else
		{
			auto point = *vertex->as<StraightVertex>();
			Vec2D pos = point.renderTranslation();

			if (auto radius = point.radius(); radius > 0.0f)
			{
				Vec2D toPrev;
				Vec2D::subtract(toPrev, Vec2D(outX, outY), pos);
				auto toPrevLength = Vec2D::length(toPrev);
				toPrev[0] /= toPrevLength;
				toPrev[1] /= toPrevLength;

				auto next = vertices[(i + 1) % length];

				Vec2D toNext;
				Vec2D::subtract(toNext,
				                next->is<CubicVertex>()
				                    ? next->as<CubicVertex>()->renderIn()
				                    : next->renderTranslation(),
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
					commandPath.cubicTo(outX,
					                    outY,
					                    translation[0],
					                    translation[1],
					                    translation[0],
					                    translation[1]);
				}
				else
				{
					commandPath.lineTo(translation[0], translation[1]);
				}

				Vec2D outPoint;
				Vec2D::scaleAndAdd(
				    outPoint, pos, toPrev, icircleConstant * renderRadius);

				Vec2D inPoint;
				Vec2D::scaleAndAdd(
				    inPoint, pos, toNext, icircleConstant * renderRadius);

				Vec2D posNext;
				Vec2D::scaleAndAdd(posNext, pos, toNext, renderRadius);
				commandPath.cubicTo(outPoint[0],
				                    outPoint[1],
				                    inPoint[0],
				                    inPoint[1],
				                    outX = posNext[0],
				                    outY = posNext[1]);
				prevIsCubic = false;
			}
			else if (prevIsCubic)
			{
				float x = pos[0];
				float y = pos[1];
				commandPath.cubicTo(outX, outY, x, y, x, y);

				prevIsCubic = false;
				outX = x;
				outY = y;
			}
			else
			{
				commandPath.lineTo(outX = pos[0], outY = pos[1]);
			}
		}
	}
	if (isClosed)
	{
		if (prevIsCubic || startIsCubic)
		{
			commandPath.cubicTo(outX, outY, startInX, startInY, startX, startY);
		}
		else
		{
			commandPath.lineTo(startX, startY);
		}
		commandPath.close();
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

void Path::onDirty(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::WorldTransform) && m_Shape != nullptr)
	{
		m_Shape->pathChanged();
	}
}

void Path::update(ComponentDirt value)
{
	Super::update(value);

	assert(m_CommandPath != nullptr);
	if (hasDirt(value, ComponentDirt::Path))
	{
		buildPath(*m_CommandPath, isPathClosed(), m_Vertices);
	}
	// if (hasDirt(value, ComponentDirt::WorldTransform) && m_Shape != nullptr)
	// {
	// 	// Make sure the path composer has an opportunity to rebuild the path
	// 	// (this is why the composer depends on the shape and all its paths,
	// 	// ascertaning it updates after both)
	// 	m_Shape->pathChanged();
	// }
}

#ifdef ENABLE_QUERY_FLAT_VERTICES

FlattenedPath* Path::makeFlat()
{
	FlattenedPath* flat = new FlattenedPath();
	for (auto vertex : m_Vertices)
	{

		switch (vertex->coreType())
		{
			case StraightVertex::typeKey:
			{
				auto straightVertex = vertex->as<StraightVertex>();
				if (straightVertex->radius() != 0)
				{
					// TODO: convert to two cubic vertices
					break;
				}
			}
			default:
				flat->m_Vertices.push_back(vertex->clone()->as<PathVertex>());
				break;
		}
	}
	return flat;
}

FlattenedPath::~FlattenedPath()
{
	for (auto vertex : m_Vertices)
	{
		delete vertex;
	}
}

#endif
