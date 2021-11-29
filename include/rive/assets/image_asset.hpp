#ifndef _RIVE_IMAGE_ASSET_HPP_
#define _RIVE_IMAGE_ASSET_HPP_

#include "rive/generated/assets/image_asset_base.hpp"
#include <string>

namespace rive
{
	class RenderImage;
	class ImageAsset : public ImageAssetBase
	{
	private:
		RenderImage* m_RenderImage;

	public:
		ImageAsset();
		~ImageAsset();

#ifdef TESTING
		std::size_t decodedByteSize = 0;
#endif
		bool decode(const uint8_t* bytes, std::size_t size) override;
		std::string fileExtension() override;
		RenderImage* renderImage() const { return m_RenderImage; }
	};
} // namespace rive

#endif