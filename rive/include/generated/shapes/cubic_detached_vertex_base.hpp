#ifndef _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_DETACHED_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicDetachedVertexBase : public CubicVertex
	{
	protected:
		typedef CubicVertex Super;

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
		float m_InRotation = 0;
		float m_InDistance = 0;
		float m_OutRotation = 0;
		float m_OutDistance = 0;
	public:
		float inRotation() const { return m_InRotation; }
		void inRotation(float value)
		{
			if (m_InRotation == value)
			{
				return;
			}
			m_InRotation = value;
			inRotationChanged();
		}

		float inDistance() const { return m_InDistance; }
		void inDistance(float value)
		{
			if (m_InDistance == value)
			{
				return;
			}
			m_InDistance = value;
			inDistanceChanged();
		}

		float outRotation() const { return m_OutRotation; }
		void outRotation(float value)
		{
			if (m_OutRotation == value)
			{
				return;
			}
			m_OutRotation = value;
			outRotationChanged();
		}

		float outDistance() const { return m_OutDistance; }
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

	protected:
		virtual void inRotationChanged() {}
		virtual void inDistanceChanged() {}
		virtual void outRotationChanged() {}
		virtual void outDistanceChanged() {}
	};
} // namespace rive

#endif