#ifndef _RIVE_FORMULA_TOKEN_OPERATION_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_OPERATION_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/formula/formula_token.hpp"
namespace rive
{
class FormulaTokenOperationBase : public FormulaToken
{
protected:
    typedef FormulaToken Super;

public:
    static const uint16_t typeKey = 541;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FormulaTokenOperationBase::typeKey:
            case FormulaTokenBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t operationTypePropertyKey = 775;

protected:
    uint32_t m_OperationType = 0;

public:
    inline uint32_t operationType() const { return m_OperationType; }
    void operationType(uint32_t value)
    {
        if (m_OperationType == value)
        {
            return;
        }
        m_OperationType = value;
        operationTypeChanged();
    }

    Core* clone() const override;
    void copy(const FormulaTokenOperationBase& object)
    {
        m_OperationType = object.m_OperationType;
        FormulaToken::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case operationTypePropertyKey:
                m_OperationType = CoreUintType::deserialize(reader);
                return true;
        }
        return FormulaToken::deserialize(propertyKey, reader);
    }

protected:
    virtual void operationTypeChanged() {}
};
} // namespace rive

#endif