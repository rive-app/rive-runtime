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

public:
    void distanceChanged() override;
    void orientChanged() override;
    StatusCode onAddedClean(CoreContext* context) override;
    const Mat2D targetTransform() const;
    void constrain(TransformComponent* component) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;

private:
    RawPath m_rawPath;
    std::vector<rcp<ContourMeasure>> m_contours;
    TransformComponents m_ComponentsA;
    TransformComponents m_ComponentsB;
};
} // namespace rive

#endif