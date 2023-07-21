#ifndef _RIVE_FOLLOW_PATH_CONSTRAINT_HPP_
#define _RIVE_FOLLOW_PATH_CONSTRAINT_HPP_
#include "rive/generated/constraints/follow_path_constraint_base.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/metrics_path.hpp"
#include <stdio.h>
namespace rive
{
class FollowPathConstraint : public FollowPathConstraintBase
{
private:
    std::unique_ptr<MetricsPath> m_WorldPath;
    TransformComponents m_ComponentsA;
    TransformComponents m_ComponentsB;

public:
    void distanceChanged() override;
    void orientChanged() override;
    StatusCode onAddedClean(CoreContext* context) override;
    const Mat2D targetTransform() const;
    void constrain(TransformComponent* component) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;
};
} // namespace rive

#endif