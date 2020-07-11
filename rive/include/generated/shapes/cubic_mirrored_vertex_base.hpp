#ifndef _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicMirroredVertexBase : public CubicVertex
	{
	public:
		static const int typeKey = 35;
		int coreType() const override { return typeKey; }
		static const int rotationPropertyKey = 82;
		static const int distancePropertyKey = 83;

	private:
		double m_Rotation = 0;
		double m_Distance = 0;
	public:
		double rotation() const { return m_Rotation; }
		void rotation(double value) { m_Rotation = value; }

		double distance() const { return m_Distance; }
		void distance(double value) { m_Distance = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case rotationPropertyKey:
					m_Rotation = CoreDoubleType::deserialize(reader);
					return true;
				case distancePropertyKey:
					m_Distance = CoreDoubleType::deserialize(reader);
					return true;
			}
			return CubicVertex::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif