#ifndef _RIVE_FOLLOW_PATH_CONSTRAINT_HPP_
#define _RIVE_FOLLOW_PATH_CONSTRAINT_HPP_
#include "rive/generated/constraints/follow_path_constraint_base.hpp"
#include "rive/shapes/metrics_path.hpp"
#include <stdio.h>
namespace rive
{
class FollowPathConstraint : public FollowPathConstraintBase
{
private:
    std::unique_ptr<MetricsPath> m_WorldPath;

public:
    void distanceChanged() override;
    void orientChanged() override;
    StatusCode onAddedClean(CoreContext* context) override;
    const Mat2D targetTransform() const override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;
};
} // namespace rive

#endif