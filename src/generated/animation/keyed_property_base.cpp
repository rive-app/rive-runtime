#include "rive/generated/animation/keyed_property_base.hpp"
#include "rive/animation/keyed_property.hpp"

using namespace rive;

Core* KeyedPropertyBase::clone() const
{
    auto cloned = new KeyedProperty();
    cloned->copy(*this);
    return cloned;
}
