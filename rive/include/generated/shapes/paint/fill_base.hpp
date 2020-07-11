#ifndef _RIVE_FILL_BASE_HPP_
#define _RIVE_FILL_BASE_HPP_
#include "core/field_types/core_int_type.hpp"
#include "shapes/paint/shape_paint.hpp"
namespace rive
{
	class FillBase : public ShapePaint
	{
	public:
		static const int typeKey = 20;
		int coreType() const override { return typeKey; }
		static const int fillRulePropertyKey = 40;

	private:
		int m_FillRule = 0;
	public:
		int fillRule() const { return m_FillRule; }
		void fillRule(int value) { m_FillRule = value; }

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case fillRulePropertyKey:
					m_FillRule = CoreIntType::deserialize(reader);
					return true;
			}
			return ShapePaint::deserialize(propertyKey, reader);
		}
	};
} // namespace rive

#endif