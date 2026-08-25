#ifndef _RIVE_FORMULA_TOKEN_ARGUMENT_SEPARATOR_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_ARGUMENT_SEPARATOR_BASE_HPP_
#include "rive/data_bind/converters/formula/formula_token.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
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

public:
    Core* clone() const override;
    void copy(const FormulaTokenArgumentSeparatorBase& object)
    {
        RIVE_EDITOR_COPY(object);
        FormulaToken::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return FormulaToken::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/data_bind/converters/formula/formula_token_argument_separator_ext.inl"
#endif
};
} // namespace rive

#endif