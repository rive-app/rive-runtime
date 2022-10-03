#ifndef _RIVE_TARGETED_CONSTRAINT_HPP_
#define _RIVE_TARGETED_CONSTRAINT_HPP_
#include "rive/generated/constraints/targeted_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
class TransformComponent;
class TargetedConstraint : public TargetedConstraintBase
{
protected:
    TransformComponent* m_Target = nullptr;

public:
    void buildDependencies() override;
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif