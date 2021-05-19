#include "generated/animation/keyed_object_base.hpp"
#include "animation/keyed_object.hpp"

using namespace rive;

Core* KeyedObjectBase::clone() const
{
	auto cloned = new KeyedObject();
	cloned->copy(*this);
	return cloned;
}
