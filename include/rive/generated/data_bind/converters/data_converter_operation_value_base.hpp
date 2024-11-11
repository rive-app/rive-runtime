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

    static const uint16_t valuePropertyKey = 681;

protected:
    float m_Value = 1.0f;

public:
    inline float value() const { return m_Value; }
    void value(float value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const DataConverterOperationValueBase& object)
    {
        m_Value = object.m_Value;
        DataConverterOperation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreDoubleType::deserialize(reader);
                return true;
        }
        return DataConverterOperation::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
};
} // namespace rive

#endif