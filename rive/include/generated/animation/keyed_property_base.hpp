#ifndef _RIVE_KEYED_PROPERTY_BASE_HPP_
#define _RIVE_KEYED_PROPERTY_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_int_type.hpp"
namespace rive
{
	class KeyedPropertyBase : public Core
	{
	public:
		static const int typeKey = 26;
		int coreType() const override { return typeKey; }
		static const int propertyKeyPropertyKey = 53;

	private:
		int m_PropertyKey = 0;
	public:
		int propertyKey() const { return m_PropertyKey; }
		void propertyKey(int value) { m_PropertyKey = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override { return false; }
	};
} // namespace rive

#endif