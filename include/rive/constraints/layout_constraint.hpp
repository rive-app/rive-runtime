#ifndef _RIVE_LAYOUT_CONSTRAINT_HPP_
#define _RIVE_LAYOUT_CONSTRAINT_HPP_
#include <stdio.h>
namespace rive
{
class LayoutNodeProvider;

class LayoutConstraint
{
public:
    virtual void constrainChild(LayoutNodeProvider* child) {}
    virtual void addLayoutChild(LayoutNodeProvider* child) {}
    virtual Constraint* constraint() = 0;
};
} // namespace rive

#endif