#ifndef _RIVE_BACKBOARD_BASE_HPP_
#define _RIVE_BACKBOARD_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class BackboardBase : public Core
	{
	public:
		static const int typeKey = 23;
		int coreType() const override { return typeKey; }
		bool deserialize(int propertyKey, BinaryReader& reader) override { return false; }
	};
} // namespace rive

#endif