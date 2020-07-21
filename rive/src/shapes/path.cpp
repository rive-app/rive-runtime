#include "shapes/path.hpp"
#include "shapes/shape.hpp"

using namespace rive;

void Path::onAddedClean(CoreContext* context)
{
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