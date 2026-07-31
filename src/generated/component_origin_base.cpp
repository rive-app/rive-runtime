#include "rive/generated/component_origin_base.hpp"
#include "rive/component_origin.hpp"

using namespace rive;

Core* ComponentOriginBase::clone() const
{
    auto cloned = new ComponentOrigin();
    cloned->copy(*this);
    return cloned;
}
