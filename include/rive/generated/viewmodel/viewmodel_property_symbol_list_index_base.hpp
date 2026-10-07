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

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif