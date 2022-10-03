#include "rive/generated/bones/weight_base.hpp"
#include "rive/bones/weight.hpp"

using namespace rive;

Core* WeightBase::clone() const
{
    auto cloned = new Weight();
    cloned->copy(*this);
    return cloned;
}
