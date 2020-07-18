#ifndef _RIVE_ANIMATION_BASE_HPP_
#define _RIVE_ANIMATION_BASE_HPP_
#include "core.hpp"
#include "core/field_types/core_string_type.hpp"
#include <string>
namespace rive
{
	class AnimationBase : public Core
	{
	public:
		static const int typeKey = 27;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case AnimationBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int namePropertyKey = 55;

	private:
		std::string m_Name;
	public:
		std::string name() const { return m_Name; }
		void name(std::string value) { m_Name = value; }

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
	};
} // namespace rive

#endif