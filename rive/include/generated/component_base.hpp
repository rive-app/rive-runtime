#ifndef _RIVE_COMPONENT_BASE_HPP_
#define _RIVE_COMPONENT_BASE_HPP_
#include "core.hpp"
#include <string>
namespace rive
{
	class ComponentBase : public Core
	{
	private:
		std::string m_Name;
		int m_ParentId = 0;
	public:
		std::string name() const { return m_Name; }
		void name(std::string value) { m_Name = value; }

		int parentId() const { return m_ParentId; }
		void parentId(int value) { m_ParentId = value; }
	};
} // namespace rive

#endif