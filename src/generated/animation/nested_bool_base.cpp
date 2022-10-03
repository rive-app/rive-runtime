#include "rive/generated/animation/nested_bool_base.hpp"
#include "rive/animation/nested_bool.hpp"

using namespace rive;

Core* NestedBoolBase::clone() const
{
    auto cloned = new NestedBool();
    cloned->copy(*this);
    return cloned;
}
