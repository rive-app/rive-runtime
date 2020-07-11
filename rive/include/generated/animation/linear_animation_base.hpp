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
	public:
		static const int typeKey = 31;
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
		double m_Speed = 1;
		int m_LoopValue = 0;
		int m_WorkStart = 0;
		int m_WorkEnd = 0;
		bool m_EnableWorkArea = false;
	public:
		int fps() const { return m_Fps; }
		void fps(int value) { m_Fps = value; }

		int duration() const { return m_Duration; }
		void duration(int value) { m_Duration = value; }

		double speed() const { return m_Speed; }
		void speed(double value) { m_Speed = value; }

		int loopValue() const { return m_LoopValue; }
		void loopValue(int value) { m_LoopValue = value; }

		int workStart() const { return m_WorkStart; }
		void workStart(int value) { m_WorkStart = value; }

		int workEnd() const { return m_WorkEnd; }
		void workEnd(int value) { m_WorkEnd = value; }

		bool enableWorkArea() const { return m_EnableWorkArea; }
		void enableWorkArea(bool value) { m_EnableWorkArea = value; }

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
	};
} // namespace rive

#endif