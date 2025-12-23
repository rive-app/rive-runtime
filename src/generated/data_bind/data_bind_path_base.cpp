#include "rive/generated/data_bind/data_bind_path_base.hpp"
#include "rive/data_bind/data_bind_path.hpp"

using namespace rive;

Core* DataBindPathBase::clone() const
{
    auto cloned = new DataBindPath();
    cloned->copy(*this);
    return cloned;
}
