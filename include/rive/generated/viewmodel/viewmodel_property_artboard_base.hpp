#ifndef _RIVE_VIEW_MODEL_PROPERTY_ARTBOARD_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_ARTBOARD_BASE_HPP_
#include "rive/viewmodel/viewmodel_property.hpp"
namespace rive
{
class ViewModelPropertyArtboardBase : public ViewModelProperty
{
protected:
    typedef ViewModelProperty Super;

public:
    static const uint16_t typeKey = 598;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif