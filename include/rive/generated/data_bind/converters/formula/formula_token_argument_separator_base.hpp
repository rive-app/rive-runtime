#ifndef _RIVE_FORMULA_TOKEN_ARGUMENT_SEPARATOR_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_ARGUMENT_SEPARATOR_BASE_HPP_
#include "rive/data_bind/converters/formula/formula_token.hpp"
namespace rive
{
class FormulaTokenArgumentSeparatorBase : public FormulaToken
{
protected:
    typedef FormulaToken Super;

public:
    static const uint16_t typeKey = 538;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FormulaTokenArgumentSeparatorBase::typeKey:
            case FormulaTokenBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif