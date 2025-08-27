#include "rive/generated/shapes/list_path_base.hpp"
#include "rive/shapes/list_path.hpp"

using namespace rive;

Core* ListPathBase::clone() const
{
    auto cloned = new ListPath();
    cloned->copy(*this);
    return cloned;
}
