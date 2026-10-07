#ifndef _RIVE_FORMULA_TOKEN_PARENTHESIS_OPEN_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_PARENTHESIS_OPEN_BASE_HPP_
#include "rive/data_bind/converters/formula/formula_token_parenthesis.hpp"
namespace rive
{
class FormulaTokenParenthesisOpenBase : public FormulaTokenParenthesis
{
protected:
    typedef FormulaTokenParenthesis Super;

public:
    static const uint16_t typeKey = 544;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif