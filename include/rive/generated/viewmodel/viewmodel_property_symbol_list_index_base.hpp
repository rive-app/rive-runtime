#ifndef _RIVE_VIEW_MODEL_PROPERTY_SYMBOL_LIST_INDEX_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_SYMBOL_LIST_INDEX_BASE_HPP_
#include "rive/viewmodel/viewmodel_property_symbol.hpp"
namespace rive
{
class ViewModelPropertySymbolListIndexBase : public ViewModelPropertySymbol
{
protected:
    typedef ViewModelPropertySymbol Super;

public:
    static const uint16_t typeKey = 564;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertySymbolListIndexBase::typeKey:
            case ViewModelPropertySymbolBase::typeKey:
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
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