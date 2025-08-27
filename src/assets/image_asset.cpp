#include "rive/assets/image_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/assets/file_asset_referencer.hpp"

using namespace rive;

ImageAsset::~ImageAsset()
{
#if defined(__EMSCRIPTEN__)
    if (m_RenderImage != nullptr)
    {
        m_RenderImage->delegate(nullptr);
    }
#endif
}

#if defined(__EMSCRIPTEN__)
void ImageAsset::decodedAsync()
{
    for (auto ref : fileAssetReferencers())
    {
        ref->assetUpdated();
    }
}
#endif

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
#if defined(__EMSCRIPTEN__)
    if (m_RenderImage != nullptr)
    {
        m_RenderImage->delegate(this);
    }
#endif
    for (auto ref : fileAssetReferencers())
    {
        ref->assetUpdated();
    }
}

std::string ImageAsset::fileExtension() const { return "png"; }
