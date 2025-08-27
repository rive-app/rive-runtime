#ifndef _RIVE_LIST_CONSTRAINT_HPP_
#define _RIVE_LIST_CONSTRAINT_HPP_
#include <stdio.h>
namespace rive
{
class ConstrainableList;

class ListConstraint
{
public:
    static ListConstraint* from(Component* component);
    virtual void constrainList(ConstrainableList* child) {}
};
} // namespace rive

#endif