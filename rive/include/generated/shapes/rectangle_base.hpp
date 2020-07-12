#ifndef _RIVE_RECTANGLE_BASE_HPP_
#define _RIVE_RECTANGLE_BASE_HPP_
#include "core/field_types/core_double_type.hpp"
#include "shapes/parametric_path.hpp"
namespace rive
{
	class RectangleBase : public ParametricPath
	{
	public:
		static const int typeKey = 7;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool inheritsFrom(int typeKey) override
		{
			switch (typeKey)
			{
				case ParametricPathBase::typeKey:
				case PathBase::typeKey:
				case NodeBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int cornerRadiusPropertyKey = 31;

	private:
		double m_CornerRadius = 0.0;
	public:
		double cornerRadius() const { return m_CornerRadius; }
		void cornerRadius(double value) { m_CornerRadius = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case cornerRadiusPropertyKey:
					m_CornerRadius = CoreDoubleType::deserialize(reader);
					return true;
			}
			return ParametricPath::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif