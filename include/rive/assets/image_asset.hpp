#ifndef _RIVE_IMAGE_ASSET_HPP_
#define _RIVE_IMAGE_ASSET_HPP_

#include "rive/generated/assets/image_asset_base.hpp"
#include "rive/renderer.hpp"
#include <string>

namespace rive
{
class ImageAsset : public ImageAssetBase
{
private:
    std::unique_ptr<RenderImage> m_RenderImage;

public:
    ImageAsset() {}
    ~ImageAsset() override;

#ifdef TESTING
    std::size_t decodedByteSize = 0;
#endif
    bool decode(Span<const uint8_t>, Factory*) override;
    std::string fileExtension() override;
    RenderImage* renderImage() const { return m_RenderImage.get(); }
    void renderImage(std::unique_ptr<RenderImage> renderImage);
};
} // namespace rive

#endif