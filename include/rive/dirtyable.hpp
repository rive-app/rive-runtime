#ifndef _RIVE_DIRTYABLE_HPP_
#define _RIVE_DIRTYABLE_HPP_

#include "rive/component_dirt.hpp"

namespace rive
{

class Dirtyable
{

public:
    virtual void addDirt(ComponentDirt value, bool recurse) = 0;
};
} // namespace rive
#endif
