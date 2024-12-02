#ifndef _RIVE_LAYOUT_CONSTRAINT_HPP_
#define _RIVE_LAYOUT_CONSTRAINT_HPP_
#include <stdio.h>
namespace rive
{
class LayoutComponent;

class LayoutConstraint
{
public:
    virtual void constrainChild(LayoutComponent* component) {}
};
} // namespace rive

#endif