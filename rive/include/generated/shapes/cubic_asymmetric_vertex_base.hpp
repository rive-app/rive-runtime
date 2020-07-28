#ifndef _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_ASYMMETRIC_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicAsymmetricVertexBase : public CubicVertex
	{
	protected:
		typedef CubicVertex Super;

	public:
		static const int typeKey = 34;

		// Helper to quickly determine if a core object extends another without
		// RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case CubicAsymmetricVertexBase::typeKey:
				case CubicVertexBase::typeKey:
				case PathVertexBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int rotationPropertyKey = 79;
		static const int inDistancePropertyKey = 80;
		static const int outDistancePropertyKey = 81;

	private:
		float m_Rotation = 0;
		float m_InDistance = 0;
		float m_OutDistance = 0;
	public:
		inline float rotation() const { return m_Rotation; }
		void rotation(float value)
		{
			if (m_Rotation == value)
			{
				return;
			}
			m_Rotation = value;
			rotationChanged();
		}

		inline float inDistance() const { return m_InDistance; }
		void inDistance(float value)
		{
			if (m_InDistance == value)
			{
				return;
			}
			m_InDistance = value;
			inDistanceChanged();
		}

		inline float outDistance() const { return m_OutDistance; }
		void outDistance(float value)
		{
			if (m_OutDistance == value)
			{
				return;
			}
			m_OutDistance = value;
			outDistanceChanged();
		}

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

	protected:
		virtual void rotationChanged() {}
		virtual void inDistanceChanged() {}
		virtual void outDistanceChanged() {}
	};
} // namespace rive

#endif