#ifndef _RIVE_KEYED_OBJECT_BASE_HPP_
#define _RIVE_KEYED_OBJECT_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_int_type.hpp"
namespace rive
{
	class KeyedObjectBase : public Core
	{
	public:
		static const int typeKey = 25;
		int coreType() const override { return typeKey; }
		static const int objectIdPropertyKey = 51;

	private:
		int m_ObjectId = 0;
	public:
		int objectId() const { return m_ObjectId; }
		void objectId(int value) { m_ObjectId = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override { return false; }
	};
} // namespace rive

#endif