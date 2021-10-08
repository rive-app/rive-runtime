#ifndef _RIVE_NESTED_ANIMATION_BASE_HPP_
#define _RIVE_NESTED_ANIMATION_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
	class NestedAnimationBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const uint16_t typeKey = 93;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case NestedAnimationBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t animationIdPropertyKey = 198;

	private:
		int m_AnimationId = -1;
	public:
		inline int animationId() const { return m_AnimationId; }
		void animationId(int value)
		{
			if (m_AnimationId == value)
			{
				return;
			}
			m_AnimationId = value;
			animationIdChanged();
		}

		void copy(const NestedAnimationBase& object)
		{
			m_AnimationId = object.m_AnimationId;
			Component::copy(object);
		}

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case animationIdPropertyKey:
					m_AnimationId = CoreUintType::deserialize(reader);
					return true;
			}
			return Component::deserialize(propertyKey, reader);
		}

	protected:
		virtual void animationIdChanged() {}
	};
} // namespace rive

#endif