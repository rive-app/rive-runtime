#ifndef _RIVE_FOLLOW_PATH_CONSTRAINT_HPP_
#define _RIVE_FOLLOW_PATH_CONSTRAINT_HPP_
#include "rive/constraints/list_constraint.hpp"
#include "rive/generated/constraints/follow_path_constraint_base.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/math/path_measure.hpp"
namespace rive
{
class ConstrainableList;
class FollowPathConstraint : public FollowPathConstraintBase,
                             public ListConstraint
{
public:
    void distanceChanged() override;
    void orientChanged() override;
    StatusCode onAddedClean(CoreContext* context) override;
    const Mat2D targetTransform(float distanceOffset = 1.0) const;
    void constrain(TransformComponent* component) override;
    void constrainList(ConstrainableList* list) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;

private:
    RawPath m_rawPath;
    PathMeasure m_pathMeasure;

    TransformComponents constrainAtOffset(const Mat2D& componentTransform,
                                          const Mat2D& parentTransform,
                                          float componentOffset);
    TransformComponents constrainHelper(const Mat2D& componentTransform,
                                        Mat2D& transformB,
                                        const Mat2D& componentParentWorld);
};
} // namespace rive

#endif