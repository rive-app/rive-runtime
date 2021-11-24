#include "rive/assets/image_asset.hpp"
#include "rive/renderer.hpp"

using namespace rive;

ImageAsset::ImageAsset() : m_RenderImage(makeRenderImage()) {}

ImageAsset::~ImageAsset() { delete m_RenderImage; }
void ImageAsset::decode(const uint8_t* bytes) { m_RenderImage->decode(bytes); }