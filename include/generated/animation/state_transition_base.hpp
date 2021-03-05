#ifndef _RIVE_STATE_TRANSITION_BASE_HPP_
#define _RIVE_STATE_TRANSITION_BASE_HPP_
#include "animation/state_machine_layer_component.hpp"
#include "core/field_types/core_uint_type.hpp"
namespace rive
{
	class StateTransitionBase : public StateMachineLayerComponent
	{
	protected:
		typedef StateMachineLayerComponent Super;

	public:
		static const int typeKey = 65;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StateTransitionBase::typeKey:
				case StateMachineLayerComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int stateToIdPropertyKey = 151;
		static const int flagsPropertyKey = 152;
		static const int durationPropertyKey = 158;

	private:
		int m_StateToId = 0;
		int m_Flags = 0;
		int m_Duration = 0;
	public:
		inline int stateToId() const { return m_StateToId; }
		void stateToId(int value)
		{
			if (m_StateToId == value)
			{
				return;
			}
			m_StateToId = value;
			stateToIdChanged();
		}

		inline int flags() const { return m_Flags; }
		void flags(int value)
		{
			if (m_Flags == value)
			{
				return;
			}
			m_Flags = value;
			flagsChanged();
		}

		inline int duration() const { return m_Duration; }
		void duration(int value)
		{
			if (m_Duration == value)
			{
				return;
			}
			m_Duration = value;
			durationChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case stateToIdPropertyKey:
					m_StateToId = CoreUintType::deserialize(reader);
					return true;
				case flagsPropertyKey:
					m_Flags = CoreUintType::deserialize(reader);
					return true;
				case durationPropertyKey:
					m_Duration = CoreUintType::deserialize(reader);
					return true;
			}
			return StateMachineLayerComponent::deserialize(propertyKey, reader);
		}

	protected:
		virtual void stateToIdChanged() {}
		virtual void flagsChanged() {}
		virtual void durationChanged() {}
	};
} // namespace rive

#endif