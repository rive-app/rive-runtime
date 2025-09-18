#ifndef _RIVE_LIST_FOLLOW_PATH_CONSTRAINT_HPP_
#define _RIVE_LIST_FOLLOW_PATH_CONSTRAINT_HPP_
#include "rive/constraints/list_constraint.hpp"
#include "rive/generated/constraints/list_follow_path_constraint_base.hpp"
#include <stdio.h>
namespace rive
{
class ConstrainableList;
class TransformComponents;

class ListFollowPathConstraint : public ListFollowPathConstraintBase,
                                 public ListConstraint
{
public:
    void distanceEndChanged() override;
    void distanceOffsetChanged() override;
    void constrainList(ConstrainableList* list) override;
    void buildDependencies() override;

private:
    TransformComponents constrainAtOffset(const Mat2D& componentTransform,
                                          const Mat2D& parentTransform,
                                          float componentOffset);
};
} // namespace rive

#endif