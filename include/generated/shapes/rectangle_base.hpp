#ifndef _RIVE_RECTANGLE_BASE_HPP_
#define _RIVE_RECTANGLE_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/parametric_path.hpp"
namespace rive
{
	class RectangleBase : public ParametricPath
	{
	protected:
		typedef ParametricPath Super;

	public:
		static const uint16_t typeKey = 7;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case RectangleBase::typeKey:
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

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t cornerRadiusPropertyKey = 31;

	private:
		float m_CornerRadius = 0;
	public:
		inline float cornerRadius() const { return m_CornerRadius; }
		void cornerRadius(float value)
		{
			if (m_CornerRadius == value)
			{
				return;
			}
			m_CornerRadius = value;
			cornerRadiusChanged();
		}

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case cornerRadiusPropertyKey:
					m_CornerRadius = CoreDoubleType::deserialize(reader);
					return true;
			}
			return ParametricPath::deserialize(propertyKey, reader);
		}

	protected:
		virtual void cornerRadiusChanged() {}
	};
} // namespace rive

#endif