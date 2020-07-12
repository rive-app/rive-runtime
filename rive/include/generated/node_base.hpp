#ifndef _RIVE_NODE_BASE_HPP_
#define _RIVE_NODE_BASE_HPP_
#include "container_component.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class NodeBase : public ContainerComponent
	{
	public:
		static const int typeKey = 2;

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

		static const int xPropertyKey = 13;
		static const int yPropertyKey = 14;
		static const int rotationPropertyKey = 15;
		static const int scaleXPropertyKey = 16;
		static const int scaleYPropertyKey = 17;
		static const int opacityPropertyKey = 18;

	private:
		double m_X = 0;
		double m_Y = 0;
		double m_Rotation = 0;
		double m_ScaleX = 1;
		double m_ScaleY = 1;
		double m_Opacity = 1;
	public:
		double x() const { return m_X; }
		void x(double value) { m_X = value; }

		double y() const { return m_Y; }
		void y(double value) { m_Y = value; }

		double rotation() const { return m_Rotation; }
		void rotation(double value) { m_Rotation = value; }

		double scaleX() const { return m_ScaleX; }
		void scaleX(double value) { m_ScaleX = value; }

		double scaleY() const { return m_ScaleY; }
		void scaleY(double value) { m_ScaleY = value; }

		double opacity() const { return m_Opacity; }
		void opacity(double value) { m_Opacity = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case xPropertyKey:
					m_X = CoreDoubleType::deserialize(reader);
					return true;
				case yPropertyKey:
					m_Y = CoreDoubleType::deserialize(reader);
					return true;
				case rotationPropertyKey:
					m_Rotation = CoreDoubleType::deserialize(reader);
					return true;
				case scaleXPropertyKey:
					m_ScaleX = CoreDoubleType::deserialize(reader);
					return true;
				case scaleYPropertyKey:
					m_ScaleY = CoreDoubleType::deserialize(reader);
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