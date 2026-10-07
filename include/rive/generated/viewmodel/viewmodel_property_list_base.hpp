#ifndef _RIVE_VIEW_MODEL_PROPERTY_LIST_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_LIST_BASE_HPP_
#include "rive/viewmodel/viewmodel_property.hpp"
namespace rive
{
class ViewModelPropertyListBase : public ViewModelProperty
{
protected:
    typedef ViewModelProperty Super;

public:
    static const uint16_t typeKey = 434;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif