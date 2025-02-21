#ifndef _RIVE_FORMULA_TOKEN_BASE_HPP_
#define _RIVE_FORMULA_TOKEN_BASE_HPP_
#include "rive/core.hpp"
namespace rive
{
class FormulaTokenBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 537;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FormulaTokenBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
    void copy(const FormulaTokenBase& object) {}

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        return false;
    }

protected:
};
} // namespace rive

#endif