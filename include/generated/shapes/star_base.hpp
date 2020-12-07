#ifndef _RIVE_STAR_BASE_HPP_
#define _RIVE_STAR_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/polygon.hpp"
namespace rive
{
	class StarBase : public Polygon
	{
	protected:
		typedef Polygon Super;

	public:
		static const int typeKey = 52;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case StarBase::typeKey:
				case PolygonBase::typeKey:
				case ParametricPathBase::typeKey:
				case PathBase::typeKey:
				case NodeBase::typeKey:
				case TransformComponentBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int innerRadiusPropertyKey = 127;

	private:
		float m_InnerRadius = 0.5;
	public:
		inline float innerRadius() const { return m_InnerRadius; }
		void innerRadius(float value)
		{
			if (m_InnerRadius == value)
			{
				return;
			}
			m_InnerRadius = value;
			innerRadiusChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case innerRadiusPropertyKey:
					m_InnerRadius = CoreDoubleType::deserialize(reader);
					return true;
			}
			return Polygon::deserialize(propertyKey, reader);
		}

	protected:
		virtual void innerRadiusChanged() {}
	};
} // namespace rive

#endif