#include "constraints/constraint.hpp"
#include "container_component.hpp"
#include "transform_component.hpp"
#include "core_context.hpp"

using namespace rive;

StatusCode Constraint::onAddedClean(CoreContext* context)
{
	if (!parent()->is<TransformComponent>())
	{
		return StatusCode::InvalidObject;
	}

	parent()->as<TransformComponent>()->addConstraint(this);

	return StatusCode::Ok;
}

void Constraint::markConstraintDirty()
{
	parent()->as<TransformComponent>()->markTransformDirty();
}

void Constraint::strengthChanged() { markConstraintDirty(); }

void Constraint::buildDependencies()
{
	Super::buildDependencies();
	parent()->addDependent(this);
}

void Constraint::onDirty(ComponentDirt dirt)
{
	// Whenever the constraint gets any dirt, make sure to mark the constrained
	// component dirty.
	markConstraintDirty();
}