#ifndef _RIVE_IMAGE_ASSET_HPP_
#define _RIVE_IMAGE_ASSET_HPP_

#include "rive/generated/assets/image_asset_base.hpp"
#include "rive/renderer.hpp"
#include "rive/simple_array.hpp"
#include <string>

namespace rive
{
class ImageAsset : public ImageAssetBase
{
private:
    rcp<RenderImage> m_RenderImage;

public:
    ImageAsset() {}
    ~ImageAsset() override;

#ifdef TESTING
    std::size_t decodedByteSize = 0;
#endif
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;
    RenderImage* renderImage() const { return m_RenderImage.get(); }
    void renderImage(rcp<RenderImage> renderImage);
};
} // namespace rive

#endif