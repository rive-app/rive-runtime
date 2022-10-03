#include "rive/assets/image_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

ImageAsset::~ImageAsset() {}

bool ImageAsset::decode(Span<const uint8_t> data, Factory* factory)
{
#ifdef TESTING
    decodedByteSize = data.size();
#endif
    m_RenderImage = factory->decodeImage(data);
    return m_RenderImage != nullptr;
}

void ImageAsset::renderImage(std::unique_ptr<RenderImage> renderImage)
{
    m_RenderImage = std::move(renderImage);
}

std::string ImageAsset::fileExtension() { return "png"; }
