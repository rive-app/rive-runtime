#include "rive/artboard.hpp"
#include "rive/constraints/constrainable_list.hpp"
#include "rive/constraints/list_follow_path_constraint.hpp"
#include "rive/math/transform_components.hpp"

using namespace rive;

void ListFollowPathConstraint::distanceEndChanged() { markConstraintDirty(); }
void ListFollowPathConstraint::distanceOffsetChanged()
{
    markConstraintDirty();
}

void ListFollowPathConstraint::constrainList(ConstrainableList* list)
{
    auto listTransform = list->listTransform();
    std::vector<Mat2D*> transforms;
    list->listItemTransforms(transforms);
    auto count = transforms.size();
    float startOffset = distanceOffset() + distance();
    float startToEndDistance = distanceEnd() - distance();
    float offsetDistance =
        count <= 1 ? 0 : startToEndDistance / ((float)count - 1);
    for (int i = 0; i < count; i++)
    {
        auto transform = transforms[i];
        auto transformComponents =
            constrainAtOffset(*transform,
                              listTransform,
                              startOffset + i * offsetDistance);
        auto transformB = Mat2D::compose(transformComponents);
        transform->xx(transformB.xx());
        transform->xy(transformB.xy());
        transform->yx(transformB.yx());
        transform->yy(transformB.yy());
        transform->tx(transformB.tx());
        transform->ty(transformB.ty());
    }
}

TransformComponents ListFollowPathConstraint::constrainAtOffset(
    const Mat2D& componentTransform,
    const Mat2D& parentTransform,
    float componentOffset)
{
    if (m_Target == nullptr || m_Target->isCollapsed())
    {
        return TransformComponents();
    }
    Mat2D transformB(targetTransform(componentOffset));
    auto transformComponents =
        constrainHelper(componentTransform, transformB, parentTransform);
    return transformComponents;
}

void ListFollowPathConstraint::buildDependencies()
{
    Super::buildDependencies();
    auto constrainableList = ConstrainableList::from(parent());
    if (constrainableList != nullptr)
    {
        constrainableList->addListConstraint(this);
    }
}
