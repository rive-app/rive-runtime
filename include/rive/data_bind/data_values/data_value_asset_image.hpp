#ifndef _RIVE_DATA_VALUE_ASSET_IMAGE_HPP_
#define _RIVE_DATA_VALUE_ASSET_IMAGE_HPP_
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/renderer.hpp"

#include <iostream>
namespace rive
{
class DataValueAssetImage : public DataValueInteger
{
public:
    DataValueAssetImage(uint32_t value) :
        DataValueInteger(value),
        m_fileAsset(rcp<ImageAsset>(new ImageAsset())) {};
    DataValueAssetImage() :
        DataValueInteger(-1), m_fileAsset(rcp<ImageAsset>(new ImageAsset())) {};
    static const DataType typeKey = DataType::assetImage;
    bool isTypeOf(DataType typeKey) const override
    {
        return typeKey == DataType::assetImage || typeKey == DataType::integer;
    };
    constexpr static uint32_t defaultValue = -1;
    rcp<ImageAsset> fileAsset() { return m_fileAsset; }
    void imageValue(RenderImage* image)
    {
        m_fileAsset->renderImage(ref_rcp(image));
    }
    RenderImage* imageValue() { return m_fileAsset->renderImage(); }

private:
    rcp<ImageAsset> m_fileAsset = nullptr;
};
} // namespace rive
#endif