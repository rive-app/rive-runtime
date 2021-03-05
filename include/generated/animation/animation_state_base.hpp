#ifndef _RIVE_ANIMATION_STATE_BASE_HPP_
#define _RIVE_ANIMATION_STATE_BASE_HPP_
#include "animation/layer_state.hpp"
#include "core/field_types/core_uint_type.hpp"
namespace rive
{
	class AnimationStateBase : public LayerState
	{
	protected:
		typedef LayerState Super;

	public:
		static const int typeKey = 61;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case AnimationStateBase::typeKey:
				case LayerStateBase::typeKey:
				case StateMachineLayerComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int animationIdPropertyKey = 149;

	private:
		int m_AnimationId = 0;
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

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case animationIdPropertyKey:
					m_AnimationId = CoreUintType::deserialize(reader);
					return true;
			}
			return LayerState::deserialize(propertyKey, reader);
		}

	protected:
		virtual void animationIdChanged() {}
	};
} // namespace rive

#endif