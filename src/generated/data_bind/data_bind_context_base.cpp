#include "rive/generated/data_bind/data_bind_context_base.hpp"
#include "rive/data_bind/data_bind_context.hpp"

using namespace rive;

Core* DataBindContextBase::clone() const
{
    auto cloned = new DataBindContext();
    cloned->copy(*this);
    return cloned;
}
