#include "shapes/points_path.hpp"
#include "bones/skin.hpp"

using namespace rive;

void PointsPath::buildDependencies()
{
	Super::buildDependencies();
	if (skin() != nullptr)
	{
		skin()->addDependent(this);
	}
}

void PointsPath::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Path) && skin() != nullptr)
	{
		skin()->deform(m_Vertices);
	}
	Super::update(value);
}

void PointsPath::markPathDirty()
{
	if (skin() != nullptr)
	{
		skin()->addDirt(ComponentDirt::Path);
	}
	Super::markPathDirty();
}

void PointsPath::markSkinDirty() { markPathDirty(); }