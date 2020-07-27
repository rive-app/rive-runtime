#include "shapes/paint/linear_gradient.hpp"
#include "math/vec2d.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "shapes/paint/gradient_stop.hpp"
#include "shapes/shape_paint_container.hpp"
#include "shapes/paint/color.hpp"

using namespace rive;

void LinearGradient::onAddedDirty(CoreContext* context)
{
	Super::onAddedDirty(context);
	assert(initPaintMutator(parent()));
}

bool LinearGradient::paintsInWorldSpace() const { return m_PaintsInWorldSpace; }
void LinearGradient::paintsInWorldSpace(bool value)
{
	if (m_PaintsInWorldSpace == value)
	{
		return;
	}
	m_PaintsInWorldSpace = value;
	addDirt(ComponentDirt::Paint);
}

void LinearGradient::buildDependencies()
{
	auto p = parent();
	if (p != nullptr && p->parent() != nullptr)
	{
		auto parentsParent = p->parent();
		// Parent's parent must be a shape paint container.
		assert(ShapePaintContainer::from(parentsParent) != nullptr);
		assert(parentsParent->is<Node>());

		m_ShapePaintContainer = parentsParent->as<Node>();
		parentsParent->addDependent(this);
	}
}

void LinearGradient::addStop(GradientStop* stop) { m_Stops.push_back(stop); }

static bool stopsComparer(GradientStop* a, GradientStop* b)
{
	return a->position() < b->position();
}

void LinearGradient::update(ComponentDirt value)
{
	// Do the stops need to be re-ordered?
	bool stopsChanged = hasDirt(value, ComponentDirt::Stops);
	if (stopsChanged)
	{
		std::sort(m_Stops.begin(), m_Stops.end(), stopsComparer);
	}

	bool worldTransformed = hasDirt(value & ComponentDirt::WorldTransform);
	bool localTransformed = hasDirt(value & ComponentDirt::Transform);

	// We rebuild the gradient if the gradient is dirty or we paint in world
	// space and the world space transform has changed, or the local transform
	// has changed. Local transform changes when a stop moves in local space.
	bool rebuildGradient = hasDirt(value, ComponentDirt::Paint) ||
	                       localTransformed ||
	                       (m_PaintsInWorldSpace && worldTransformed);

	if (rebuildGradient)
	{
		auto paint = renderPaint();
		Vec2D start(startX(), startY());
		Vec2D end(endX(), endY());
		// Check if we need to update the world space gradient.
		if (m_PaintsInWorldSpace)
		{
			// Get the start and end of the gradient in world coordinates (world
			// transform of the shape).
			const Mat2D& world = m_ShapePaintContainer->worldTransform();
			Vec2D worldStart;
			Vec2D::transform(worldStart, start, world);

			Vec2D worldEnd;
			Vec2D::transform(worldEnd, end, world);
			makeGradient(worldStart, worldEnd);
		}
		else
		{
			makeGradient(start, end);
		}
		// build up the color and positions lists
		double ro = opacity() * renderOpacity();
		for (auto stop : m_Stops)
		{
			paint->addStop(
			    colorModulateOpacity((unsigned int)stop->colorValue(), ro),
			    stop->position());
		}
		paint->completeGradient();
	}
}

void LinearGradient::makeGradient(const Vec2D& start, const Vec2D& end)
{
	renderPaint()->linearGradient(start[0], start[1], end[0], end[1]);
}

void LinearGradient::markGradientDirty()
{
	addDirt(ComponentDirt::Paint & ComponentDirt::Stops);
}
void LinearGradient::markStopsDirty() { addDirt(ComponentDirt::Paint); }

void LinearGradient::renderOpacityChanged() { markGradientDirty(); }

void LinearGradient::startXChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::startYChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::endXChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::endYChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::opacityChanged() { markGradientDirty(); }