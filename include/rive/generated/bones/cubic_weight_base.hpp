#ifndef _RIVE_CUBIC_WEIGHT_BASE_HPP_
#define _RIVE_CUBIC_WEIGHT_BASE_HPP_
#include "rive/bones/weight.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
	class CubicWeightBase : public Weight
	{
	protected:
		typedef Weight Super;

	public:
		static const uint16_t typeKey = 46;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case CubicWeightBase::typeKey:
				case WeightBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t inValuesPropertyKey = 110;
		static const uint16_t inIndicesPropertyKey = 111;
		static const uint16_t outValuesPropertyKey = 112;
		static const uint16_t outIndicesPropertyKey = 113;

	private:
		int m_InValues = 255;
		int m_InIndices = 1;
		int m_OutValues = 255;
		int m_OutIndices = 1;
	public:
		inline int inValues() const { return m_InValues; }
		void inValues(int value)
		{
			if (m_InValues == value)
			{
				return;
			}
			m_InValues = value;
			inValuesChanged();
		}

		inline int inIndices() const { return m_InIndices; }
		void inIndices(int value)
		{
			if (m_InIndices == value)
			{
				return;
			}
			m_InIndices = value;
			inIndicesChanged();
		}

		inline int outValues() const { return m_OutValues; }
		void outValues(int value)
		{
			if (m_OutValues == value)
			{
				return;
			}
			m_OutValues = value;
			outValuesChanged();
		}

		inline int outIndices() const { return m_OutIndices; }
		void outIndices(int value)
		{
			if (m_OutIndices == value)
			{
				return;
			}
			m_OutIndices = value;
			outIndicesChanged();
		}

		Core* clone() const override;
		void copy(const CubicWeightBase& object)
		{
			m_InValues = object.m_InValues;
			m_InIndices = object.m_InIndices;
			m_OutValues = object.m_OutValues;
			m_OutIndices = object.m_OutIndices;
			Weight::copy(object);
		}

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case inValuesPropertyKey:
					m_InValues = CoreUintType::deserialize(reader);
					return true;
				case inIndicesPropertyKey:
					m_InIndices = CoreUintType::deserialize(reader);
					return true;
				case outValuesPropertyKey:
					m_OutValues = CoreUintType::deserialize(reader);
					return true;
				case outIndicesPropertyKey:
					m_OutIndices = CoreUintType::deserialize(reader);
					return true;
			}
			return Weight::deserialize(propertyKey, reader);
		}

	protected:
		virtual void inValuesChanged() {}
		virtual void inIndicesChanged() {}
		virtual void outValuesChanged() {}
		virtual void outIndicesChanged() {}
	};
} // namespace rive

#endif