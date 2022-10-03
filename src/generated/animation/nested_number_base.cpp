#include "rive/generated/animation/nested_number_base.hpp"
#include "rive/animation/nested_number.hpp"

using namespace rive;

Core* NestedNumberBase::clone() const
{
    auto cloned = new NestedNumber();
    cloned->copy(*this);
    return cloned;
}
