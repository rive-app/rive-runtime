#include "rive/constraints/targeted_constraint.hpp"
#include "rive/transform_component.hpp"
#include "rive/core_context.hpp"

using namespace rive;

StatusCode TargetedConstraint::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(targetId());
    if (coreObject == nullptr || !coreObject->is<TransformComponent>())
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
