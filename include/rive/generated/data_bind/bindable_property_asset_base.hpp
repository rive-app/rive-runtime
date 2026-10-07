#ifndef _RIVE_BINDABLE_PROPERTY_ASSET_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_ASSET_BASE_HPP_
#include "rive/data_bind/bindable_property_id.hpp"
namespace rive
{
class BindablePropertyAssetBase : public BindablePropertyId
{
protected:
    typedef BindablePropertyId Super;

public:
    static const uint16_t typeKey = 588;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif