#ifndef _RIVE_PARAMETRIC_PATH_BASE_HPP_
#define _RIVE_PARAMETRIC_PATH_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/path.hpp"
namespace rive
{
	class ParametricPathBase : public Path
	{
	public:
		static const int typeKey = 15;
		int coreType() const override { return typeKey; }
		static const int widthPropertyKey = 20;
		static const int heightPropertyKey = 21;

	private:
		double m_Width = 0;
		double m_Height = 0;
	public:
		double width() const { return m_Width; }
		void width(double value) { m_Width = value; }

		double height() const { return m_Height; }
		void height(double value) { m_Height = value; }

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
			}
			return Path::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif