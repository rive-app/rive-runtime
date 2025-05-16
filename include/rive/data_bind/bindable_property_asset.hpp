#ifndef _RIVE_BINDABLE_PROPERTY_ASSET_HPP_
#define _RIVE_BINDABLE_PROPERTY_ASSET_HPP_
#include "rive/generated/data_bind/bindable_property_asset_base.hpp"
#include <stdio.h>
namespace rive
{
class BindablePropertyAsset : public BindablePropertyAssetBase
{
public:
    constexpr static uint32_t defaultValue = -1;
};
} // namespace rive

#endif