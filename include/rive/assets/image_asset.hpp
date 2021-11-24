#ifndef _RIVE_IMAGE_ASSET_HPP_
#define _RIVE_IMAGE_ASSET_HPP_
#include "rive/generated/assets/image_asset_base.hpp"
#include <stdio.h>
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

		void decode(const uint8_t* bytes) override;
		RenderImage* renderImage() const { return m_RenderImage; }
	};
} // namespace rive

#endif