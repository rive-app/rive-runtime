#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/path.hpp"

using namespace rive;

Vec2D PathVertex::renderTranslation()
{
	if (hasWeight())
	{
		return m_Weight->translation();
	}
	return Vec2D(x(), y());
}

StatusCode PathVertex::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	if (!parent()->is<Path>())
	{
		return StatusCode::MissingObject;
	}
	parent()->as<Path>()->addVertex(this);
	return StatusCode::Ok;
}

void PathVertex::markPathDirty()
{
	if (parent() == nullptr)
	{
		// This is an acceptable condition as the parametric paths create points
		// that are not part of the core context.
		return;
	}
	parent()->as<Path>()->markPathDirty();
}

void PathVertex::xChanged() { markPathDirty(); }
void PathVertex::yChanged() { markPathDirty(); }

void PathVertex::deform(Mat2D& worldTransform, float* boneTransforms)
{
	Weight::deform(x(),
	               y(),
	               m_Weight->indices(),
	               m_Weight->values(),
	               worldTransform,
	               boneTransforms,
	               m_Weight->translation());
}