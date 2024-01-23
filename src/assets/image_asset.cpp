#include "rive/assets/image_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

ImageAsset::~ImageAsset() {}

bool ImageAsset::decode(SimpleArray<uint8_t>& data, Factory* factory)
{
#ifdef TESTING
    decodedByteSize = data.size();
#endif
    renderImage(factory->decodeImage(data));
    return m_RenderImage != nullptr;
}

void ImageAsset::renderImage(rcp<RenderImage> renderImage)
{
    m_RenderImage = std::move(renderImage);
}

std::string ImageAsset::fileExtension() const { return "png"; }
