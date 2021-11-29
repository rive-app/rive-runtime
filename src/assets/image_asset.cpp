#include "rive/assets/image_asset.hpp"
#include "rive/renderer.hpp"

using namespace rive;

ImageAsset::ImageAsset() : m_RenderImage(makeRenderImage()) {}

ImageAsset::~ImageAsset() { delete m_RenderImage; }
bool ImageAsset::decode(const uint8_t* bytes, std::size_t size)
{
#ifdef TESTING
	decodedByteSize = size;
#endif
	return m_RenderImage->decode(bytes, size);
}

std::string ImageAsset::fileExtension() { return "png"; }