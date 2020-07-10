#ifndef _RIVE_KEYED_PROPERTY_HPP_
#define _RIVE_KEYED_PROPERTY_HPP_
#include "generated/animation/keyed_property_base.hpp"
#include <stdio.h>
namespace rive
{
	class KeyedProperty : public KeyedPropertyBase
	{
	public:
		KeyedProperty() { printf("Constructing KeyedProperty\n"); }
	};
} // namespace rive

#endif