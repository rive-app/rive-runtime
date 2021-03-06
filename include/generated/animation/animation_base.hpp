#ifndef _RIVE_ANIMATION_BASE_HPP_
#define _RIVE_ANIMATION_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_string_type.hpp"
#include <string>
namespace rive
{
	class AnimationBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const uint16_t typeKey = 27;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case AnimationBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t namePropertyKey = 55;

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

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
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