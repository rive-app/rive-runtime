#ifndef _RIVE_WEIGHT_BASE_HPP_
#define _RIVE_WEIGHT_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
	class WeightBase : public Component
	{
	protected:
		typedef Component Super;

	public:
		static const uint16_t typeKey = 45;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
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

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t valuesPropertyKey = 102;
		static const uint16_t indicesPropertyKey = 103;

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

		Core* clone() const override;
		void copy(const WeightBase& object)
		{
			m_Values = object.m_Values;
			m_Indices = object.m_Indices;
			Component::copy(object);
		}

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
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