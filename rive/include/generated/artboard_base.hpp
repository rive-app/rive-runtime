#ifndef _RIVE_ARTBOARD_BASE_HPP_
#define _RIVE_ARTBOARD_BASE_HPP_
#include "container_component.hpp"
#include "core/field_types/core_double_type.hpp"
namespace rive
{
	class ArtboardBase : public ContainerComponent
	{
	public:
		static const int typeKey = 1;
		int coreType() const override { return typeKey; }
		static const int widthPropertyKey = 7;
		static const int heightPropertyKey = 8;
		static const int xPropertyKey = 9;
		static const int yPropertyKey = 10;
		static const int originXPropertyKey = 11;
		static const int originYPropertyKey = 12;

	private:
		double m_Width = 0.0;
		double m_Height = 0.0;
		double m_X = 0.0;
		double m_Y = 0.0;
		double m_OriginX = 0.0;
		double m_OriginY = 0.0;
	public:
		double width() const { return m_Width; }
		void width(double value) { m_Width = value; }

		double height() const { return m_Height; }
		void height(double value) { m_Height = value; }

		double x() const { return m_X; }
		void x(double value) { m_X = value; }

		double y() const { return m_Y; }
		void y(double value) { m_Y = value; }

		double originX() const { return m_OriginX; }
		void originX(double value) { m_OriginX = value; }

		double originY() const { return m_OriginY; }
		void originY(double value) { m_OriginY = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case widthPropertyKey:
					m_Width = CoreDoubleType::deserialize(reader);
					return true;
				case heightPropertyKey:
					m_Height = CoreDoubleType::deserialize(reader);
					return true;
				case xPropertyKey:
					m_X = CoreDoubleType::deserialize(reader);
					return true;
				case yPropertyKey:
					m_Y = CoreDoubleType::deserialize(reader);
					return true;
				case originXPropertyKey:
					m_OriginX = CoreDoubleType::deserialize(reader);
					return true;
				case originYPropertyKey:
					m_OriginY = CoreDoubleType::deserialize(reader);
					return true;
			}
			return ContainerComponent::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif