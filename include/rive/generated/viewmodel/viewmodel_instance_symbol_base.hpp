#ifndef _RIVE_VIEW_MODEL_INSTANCE_SYMBOL_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_SYMBOL_BASE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
namespace rive
{
class ViewModelInstanceSymbolBase : public ViewModelInstanceValue
{
protected:
    typedef ViewModelInstanceValue Super;

public:
    static const uint16_t typeKey = 565;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif