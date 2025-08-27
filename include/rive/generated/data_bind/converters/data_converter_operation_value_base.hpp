#ifndef _RIVE_DATA_CONVERTER_OPERATION_VALUE_BASE_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_VALUE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/data_bind/converters/data_converter_operation.hpp"
namespace rive
{
class DataConverterOperationValueBase : public DataConverterOperation
{
protected:
    typedef DataConverterOperation Super;

public:
    static const uint16_t typeKey = 500;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterOperationValueBase::typeKey:
            case DataConverterOperationBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t operationValuePropertyKey = 681;

protected:
    float m_OperationValue = 1.0f;

public:
    inline float operationValue() const { return m_OperationValue; }
    void operationValue(float value)
    {
        if (m_OperationValue == value)
        {
            return;
        }
        m_OperationValue = value;
        operationValueChanged();
    }

    Core* clone() const override;
    void copy(const DataConverterOperationValueBase& object)
    {
        m_OperationValue = object.m_OperationValue;
        DataConverterOperation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case operationValuePropertyKey:
                m_OperationValue = CoreDoubleType::deserialize(reader);
                return true;
        }
        return DataConverterOperation::deserialize(propertyKey, reader);
    }

protected:
    virtual void operationValueChanged() {}
};
} // namespace rive

#endif