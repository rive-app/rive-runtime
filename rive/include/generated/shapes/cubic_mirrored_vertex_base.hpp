#ifndef _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_MIRRORED_VERTEX_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/cubic_vertex.hpp"
namespace rive
{
	class CubicMirroredVertexBase : public CubicVertex
	{
	protected:
		typedef CubicVertex Super;

	public:
		static const int typeKey = 35;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case CubicMirroredVertexBase::typeKey:
				case CubicVertexBase::typeKey:
				case PathVertexBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int rotationPropertyKey = 82;
		static const int distancePropertyKey = 83;

	private:
		float m_Rotation = 0;
		float m_Distance = 0;
	public:
		float rotation() const { return m_Rotation; }
		void rotation(float value)
		{
			if (m_Rotation == value)
			{
				return;
			}
			m_Rotation = value;
			rotationChanged();
		}

		float distance() const { return m_Distance; }
		void distance(float value)
		{
			if (m_Distance == value)
			{
				return;
			}
			m_Distance = value;
			distanceChanged();
		}

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

	protected:
		virtual void rotationChanged() {}
		virtual void distanceChanged() {}
	};
} // namespace rive

#endif