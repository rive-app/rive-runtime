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
};
} // namespace rive

#endif