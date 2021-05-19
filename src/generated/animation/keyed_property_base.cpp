#include "generated/animation/keyed_property_base.hpp"
#include "animation/keyed_property.hpp"

using namespace rive;

Core* KeyedPropertyBase::clone() const
{
	auto cloned = new KeyedProperty();
	cloned->copy(*this);
	return cloned;
}
