#ifndef _RIVE_STRAIGHT_VERTEX_BASE_HPP_
#define _RIVE_STRAIGHT_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/path_vertex.hpp"
namespace rive
{
	class StraightVertexBase : public PathVertex
	{
	public:
		static const int typeKey = 5;
		int coreType() const override { return typeKey; }
		static const int radiusPropertyKey = 26;

	private:
		double m_Radius = 0;
	public:
		double radius() const { return m_Radius; }
		void radius(double value) { m_Radius = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case radiusPropertyKey:
					m_Radius = CoreDoubleType::deserialize(reader);
					return true;
			}
			return PathVertex::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif