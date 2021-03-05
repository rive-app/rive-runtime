#ifndef _RIVE_STATE_MACHINE_COMPONENT_BASE_HPP_
#define _RIVE_STATE_MACHINE_COMPONENT_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_string_type.hpp"
#include <string>
namespace rive
{
	class StateMachineComponentBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const int typeKey = 54;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StateMachineComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int namePropertyKey = 138;

	private:
		std::string m_Name = "";
	public:
		inline std::string name() const { return m_Name; }
		void name(std::string value)
		{
			if (m_Name == value)
			{
				return;
			}
			m_Name = value;
			nameChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case namePropertyKey:
					m_Name = CoreStringType::deserialize(reader);
					return true;
			}
			return false;
		}

	protected:
		virtual void nameChanged() {}
	};
} // namespace rive

#endif