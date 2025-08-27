#ifndef _RIVE_FORMULA_TOKEN_FUNCTION_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_FUNCTION_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis.hpp"
namespace rive
{
class FormulaTokenFunctionBase : public FormulaTokenParenthesis
{
protected:
    typedef FormulaTokenParenthesis Super;

public:
    static const uint16_t typeKey = 542;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FormulaTokenFunctionBase::typeKey:
            case FormulaTokenParenthesisBase::typeKey:
            case FormulaTokenBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t functionTypePropertyKey = 776;

protected:
    uint32_t m_FunctionType = 0;

public:
    inline uint32_t functionType() const { return m_FunctionType; }
    void functionType(uint32_t value)
    {
        if (m_FunctionType == value)
        {
            return;
        }
        m_FunctionType = value;
        functionTypeChanged();
    }

    Core* clone() const override;
    void copy(const FormulaTokenFunctionBase& object)
    {
        m_FunctionType = object.m_FunctionType;
        FormulaTokenParenthesis::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case functionTypePropertyKey:
                m_FunctionType = CoreUintType::deserialize(reader);
                return true;
        }
        return FormulaTokenParenthesis::deserialize(propertyKey, reader);
    }

protected:
    virtual void functionTypeChanged() {}
};
} // namespace rive

#endif