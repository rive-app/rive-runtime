#ifndef _RIVE_LINEAR_GRADIENT_BASE_HPP_
#define _RIVE_LINEAR_GRADIENT_BASE_HPP_
#include "container_component.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class LinearGradientBase : public ContainerComponent
	{
	public:
		static const int typeKey = 22;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool inheritsFrom(int typeKey) override
		{
			switch (typeKey)
			{
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int startXPropertyKey = 42;
		static const int startYPropertyKey = 33;
		static const int endXPropertyKey = 34;
		static const int endYPropertyKey = 35;
		static const int opacityPropertyKey = 46;

	private:
		double m_StartX = 0;
		double m_StartY = 0;
		double m_EndX = 0;
		double m_EndY = 0;
		double m_Opacity = 1;
	public:
		double startX() const { return m_StartX; }
		void startX(double value) { m_StartX = value; }

		double startY() const { return m_StartY; }
		void startY(double value) { m_StartY = value; }

		double endX() const { return m_EndX; }
		void endX(double value) { m_EndX = value; }

		double endY() const { return m_EndY; }
		void endY(double value) { m_EndY = value; }

		double opacity() const { return m_Opacity; }
		void opacity(double value) { m_Opacity = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case startXPropertyKey:
					m_StartX = CoreDoubleType::deserialize(reader);
					return true;
				case startYPropertyKey:
					m_StartY = CoreDoubleType::deserialize(reader);
					return true;
				case endXPropertyKey:
					m_EndX = CoreDoubleType::deserialize(reader);
					return true;
				case endYPropertyKey:
					m_EndY = CoreDoubleType::deserialize(reader);
					return true;
				case opacityPropertyKey:
					m_Opacity = CoreDoubleType::deserialize(reader);
					return true;
			}
			return ContainerComponent::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif