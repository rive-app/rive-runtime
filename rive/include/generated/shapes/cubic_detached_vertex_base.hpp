#ifndef _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicDetachedVertexBase : public CubicVertex
	{
	public:
		static const int typeKey = 6;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case CubicDetachedVertexBase::typeKey:
				case CubicVertexBase::typeKey:
				case PathVertexBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int inRotationPropertyKey = 84;
		static const int inDistancePropertyKey = 85;
		static const int outRotationPropertyKey = 86;
		static const int outDistancePropertyKey = 87;

	private:
		double m_InRotation = 0;
		double m_InDistance = 0;
		double m_OutRotation = 0;
		double m_OutDistance = 0;
	public:
		double inRotation() const { return m_InRotation; }
		void inRotation(double value) { m_InRotation = value; }

		double inDistance() const { return m_InDistance; }
		void inDistance(double value) { m_InDistance = value; }

		double outRotation() const { return m_OutRotation; }
		void outRotation(double value) { m_OutRotation = value; }

		double outDistance() const { return m_OutDistance; }
		void outDistance(double value) { m_OutDistance = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case inRotationPropertyKey:
					m_InRotation = CoreDoubleType::deserialize(reader);
					return true;
				case inDistancePropertyKey:
					m_InDistance = CoreDoubleType::deserialize(reader);
					return true;
				case outRotationPropertyKey:
					m_OutRotation = CoreDoubleType::deserialize(reader);
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