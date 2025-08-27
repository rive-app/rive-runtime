#ifndef _RIVE_CONSTRAINABLE_LIST_HPP_
#define _RIVE_CONSTRAINABLE_LIST_HPP_
#include <stdio.h>
#include <vector>
namespace rive
{
class Component;
class ListConstraint;

class ConstrainableList
{
protected:
    std::vector<ListConstraint*> m_listConstraints;

public:
    static ConstrainableList* from(Component* component);
    void addListConstraint(ListConstraint* constraint);
    virtual const Mat2D& listTransform() = 0;
    virtual void listItemTransforms(std::vector<Mat2D*>& transforms) = 0;
};
} // namespace rive

#endif