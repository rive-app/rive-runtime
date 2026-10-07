#ifndef _RIVE_VIEW_MODEL_PROPERTY_SYMBOL_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_SYMBOL_BASE_HPP_
#include "rive/viewmodel/viewmodel_property.hpp"
namespace rive
{
class ViewModelPropertySymbolBase : public ViewModelProperty
{
protected:
    typedef ViewModelProperty Super;

public:
    static const uint16_t typeKey = 563;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif