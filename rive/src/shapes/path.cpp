#include "shapes/path.hpp"
#include "renderer.hpp"
#include "shapes/cubic_vertex.hpp"
#include "shapes/path_vertex.hpp"
#include "shapes/shape.hpp"
#include "shapes/straight_vertex.hpp"

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
	for (auto currentParent = parent(); currentParent != nullptr; currentParent = currentParent->parent())
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

void Path::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path))
	{
		// TODO: build the render path...
		m_RenderPath->reset();

		const float arcConstant = 0.55f;
		const float iarcConstant = 1.0f - arcConstant;
		auto length = m_Vertices.size();
		if (length == 0)
		{
			return;
		}
		auto firstPoint = m_Vertices[0];

		// Init out to translation
		float outX = firstPoint->x(), outY = firstPoint->y();
		m_RenderPath->moveTo(outX, outY);
		bool isCubic = false;

		if (firstPoint->is<CubicVertex>())
		{
			auto cubic = firstPoint->as<CubicVertex>();
			isCubic = true;
			auto outPoint = cubic->outPoint();
			outX = outPoint[0];
			outY = outPoint[1];
		}

		for (int i = 1; i < length; i++)
		{
			auto vertex = m_Vertices[i];

			if (vertex->is<CubicVertex>())
			{
				auto cubic = vertex->as<CubicVertex>();
				auto inPoint = cubic->inPoint();

				m_RenderPath->cubicTo(outX, outY, inPoint[0], inPoint[1], cubic->x(), cubic->y());

				isCubic = true;
				auto outPoint = cubic->outPoint();
				outX = outPoint[0];
				outY = outPoint[1];
			}
			else
			{
				auto point = vertex->as<StraightVertex>();

				if(point->radius() > 0.0f)
				{
					
				}
				if (isCubic)
				{
					float x = point->x();
					float y = point->y();
					m_RenderPath->cubicTo(outX, outY, x, y, x, y);

					isCubic = false;
					outX = x;
					outY = y;
				}
				else
				{
					m_RenderPath->lineTo(point->x(), point->y());
				}
			}
		}
	}
}