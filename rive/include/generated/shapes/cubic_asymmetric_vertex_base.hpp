#ifndef _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicAsymmetricVertexBase : public CubicVertex
	{
	public:
		static const int typeKey = 34;
		int coreType() const override { return typeKey; }
		static const int rotationPropertyKey = 79;
		static const int inDistancePropertyKey = 80;
		static const int outDistancePropertyKey = 81;

	private:
		double m_Rotation = 0;
		double m_InDistance = 0;
		double m_OutDistance = 0;
	public:
		double rotation() const { return m_Rotation; }
		void rotation(double value) { m_Rotation = value; }

		double inDistance() const { return m_InDistance; }
		void inDistance(double value) { m_InDistance = value; }

		double outDistance() const { return m_OutDistance; }
		void outDistance(double value) { m_OutDistance = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case rotationPropertyKey:
					m_Rotation = CoreDoubleType::deserialize(reader);
					return true;
				case inDistancePropertyKey:
					m_InDistance = CoreDoubleType::deserialize(reader);
					return true;
				case outDistancePropertyKey:
					m_OutDistance = CoreDoubleType::deserialize(reader);
					return true;
			}
			return CubicVertex::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif