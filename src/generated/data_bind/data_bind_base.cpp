#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/data_bind/data_bind.hpp"

using namespace rive;

Core* DataBindBase::clone() const
{
    auto cloned = new DataBind();
    cloned->copy(*this);
    return cloned;
}
