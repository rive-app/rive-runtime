#ifndef _RIVE_KEYED_PROPERTY_BASE_HPP_
#define _RIVE_KEYED_PROPERTY_BASE_HPP_
#include "core.hpp"
namespace rive
{
	class KeyedPropertyBase : public Core
	{
	private:
		int m_PropertyKey = 0;
	public:
		int propertyKey() const { return m_PropertyKey; }
		void propertyKey(int value) { m_PropertyKey = value; }
	};
} // namespace rive

#endif