#ifndef _RIVE_WEIGHT_BASE_HPP_
#define _RIVE_WEIGHT_BASE_HPP_
#include "component.hpp"
#include "core/field_types/core_uint_type.hpp"
namespace rive
{
	class WeightBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const int typeKey = 45;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(int typeKey) const override
		{
			switch (typeKey)
			{
				case WeightBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

		static const int valuesPropertyKey = 102;
		static const int indicesPropertyKey = 103;

	private:
		int m_Values = 255;
		int m_Indices = 1;
	public:
		inline int values() const { return m_Values; }
		void values(int value)
		{
			if (m_Values == value)
			{
				return;
			}
			m_Values = value;
			valuesChanged();
		}

		inline int indices() const { return m_Indices; }
		void indices(int value)
		{
			if (m_Indices == value)
			{
				return;
			}
			m_Indices = value;
			indicesChanged();
		}

		bool deserialize(int propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case valuesPropertyKey:
					m_Values = CoreUintType::deserialize(reader);
					return true;
				case indicesPropertyKey:
					m_Indices = CoreUintType::deserialize(reader);
					return true;
			}
			return Component::deserialize(propertyKey, reader);
		}

	protected:
		virtual void valuesChanged() {}
		virtual void indicesChanged() {}
	};
} // namespace rive

#endif