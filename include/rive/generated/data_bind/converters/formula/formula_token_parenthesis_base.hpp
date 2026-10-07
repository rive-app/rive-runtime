#ifndef _RIVE_FORMULA_TOKEN_PARENTHESIS_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_PARENTHESIS_BASE_HPP_
#include "rive/data_bind/converters/formula/formula_token.hpp"
namespace rive
{
class FormulaTokenParenthesisBase : public FormulaToken
{
protected:
    typedef FormulaToken Super;

public:
    static const uint16_t typeKey = 539;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif