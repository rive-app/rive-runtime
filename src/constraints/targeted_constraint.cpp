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
    // If core object is not null and is not a TransformComponent it should
    // always be invalid
    if (coreObject != nullptr && !coreObject->is<TransformComponent>())
    {
        return false;
    }
    return !requiresTarget() || coreObject != nullptr;
}

StatusCode TargetedConstraint::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(targetId());
    if (requiresTarget() && coreObject == nullptr)
    {
        return StatusCode::MissingObject;
    }

    m_Target = static_cast<TransformComponent*>(coreObject);

    return StatusCode::Ok;
}

void TargetedConstraint::buildDependencies()
{
    // Targeted constraints must have their constrained component (parent)
    // update after the target.
    if (m_Target != nullptr)
    {
        m_Target->addDependent(parent());
    }
}
