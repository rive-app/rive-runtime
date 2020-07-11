#ifndef _RIVE_KEYED_OBJECT_BASE_HPP_
#define _RIVE_KEYED_OBJECT_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class KeyedObjectBase : public Core
	{
	private:
		int m_ObjectId = 0;
	public:
		int objectId() const { return m_ObjectId; }
		void objectId(int value) { m_ObjectId = value; }
	};
} // namespace rive

#endif