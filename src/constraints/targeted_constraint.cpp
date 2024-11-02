#include "rive/constraints/targeted_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/core_context.hpp"

using namespace rive;

bool TargetedConstraint::validate(CoreContext* context)
{
    if (!Super::validate(context))
    {
        return false;
    }
    auto coreObject = context->resolve(targetId());
    return coreObject != nullptr && coreObject->is<TransformComponent>();
}

StatusCode TargetedConstraint::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    m_Target = context->resolve(targetId())->as<TransformComponent>();

    return StatusCode::Ok;
}

void TargetedConstraint::buildDependencies()
{
    // Targeted constraints must have their constrained component (parent)
    // update after the target.
    m_Target->addDependent(parent());
}
