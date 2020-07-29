#ifndef _RIVE_LINEAR_ANIMATION_BASE_HPP_
#define _RIVE_LINEAR_ANIMATION_BASE_HPP_
#include "animation/animation.hpp"
#include "core/field_types/core_bool_type.hpp"
#include "core/field_types/core_double_type.hpp"
#include "core/field_types/core_int_type.hpp"
namespace rive
{
	class LinearAnimationBase : public Animation
	{
	protected:
		typedef Animation Super;

	public:
		static const int typeKey = 31;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case LinearAnimationBase::typeKey:
				case AnimationBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int fpsPropertyKey = 56;
		static const int durationPropertyKey = 57;
		static const int speedPropertyKey = 58;
		static const int loopValuePropertyKey = 59;
		static const int workStartPropertyKey = 60;
		static const int workEndPropertyKey = 61;
		static const int enableWorkAreaPropertyKey = 62;

	private:
		int m_Fps = 60;
		int m_Duration = 60;
		float m_Speed = 1;
		int m_LoopValue = 0;
		int m_WorkStart = 0;
		int m_WorkEnd = 0;
		bool m_EnableWorkArea = false;
	public:
		inline int fps() const { return m_Fps; }
		void fps(int value)
		{
			if (m_Fps == value)
			{
				return;
			}
			m_Fps = value;
			fpsChanged();
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

		inline float speed() const { return m_Speed; }
		void speed(float value)
		{
			if (m_Speed == value)
			{
				return;
			}
			m_Speed = value;
			speedChanged();
		}

		inline int loopValue() const { return m_LoopValue; }
		void loopValue(int value)
		{
			if (m_LoopValue == value)
			{
				return;
			}
			m_LoopValue = value;
			loopValueChanged();
		}

		inline int workStart() const { return m_WorkStart; }
		void workStart(int value)
		{
			if (m_WorkStart == value)
			{
				return;
			}
			m_WorkStart = value;
			workStartChanged();
		}

		inline int workEnd() const { return m_WorkEnd; }
		void workEnd(int value)
		{
			if (m_WorkEnd == value)
			{
				return;
			}
			m_WorkEnd = value;
			workEndChanged();
		}

		inline bool enableWorkArea() const { return m_EnableWorkArea; }
		void enableWorkArea(bool value)
		{
			if (m_EnableWorkArea == value)
			{
				return;
			}
			m_EnableWorkArea = value;
			enableWorkAreaChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case fpsPropertyKey:
					m_Fps = CoreIntType::deserialize(reader);
					return true;
				case durationPropertyKey:
					m_Duration = CoreIntType::deserialize(reader);
					return true;
				case speedPropertyKey:
					m_Speed = CoreDoubleType::deserialize(reader);
					return true;
				case loopValuePropertyKey:
					m_LoopValue = CoreIntType::deserialize(reader);
					return true;
				case workStartPropertyKey:
					m_WorkStart = CoreIntType::deserialize(reader);
					return true;
				case workEndPropertyKey:
					m_WorkEnd = CoreIntType::deserialize(reader);
					return true;
				case enableWorkAreaPropertyKey:
					m_EnableWorkArea = CoreBoolType::deserialize(reader);
					return true;
			}
			return Animation::deserialize(propertyKey, reader);
		}

	protected:
		virtual void fpsChanged() {}
		virtual void durationChanged() {}
		virtual void speedChanged() {}
		virtual void loopValueChanged() {}
		virtual void workStartChanged() {}
		virtual void workEndChanged() {}
		virtual void enableWorkAreaChanged() {}
	};
} // namespace rive

#endif