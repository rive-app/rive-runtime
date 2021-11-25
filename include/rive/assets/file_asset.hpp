#ifndef _RIVE_FILE_ASSET_HPP_
#define _RIVE_FILE_ASSET_HPP_
#include "rive/generated/assets/file_asset_base.hpp"
#include <stdio.h>
namespace rive
{
	class FileAsset : public FileAssetBase
	{
	public:
		virtual void decode(const uint8_t* bytes, std::size_t size) = 0;
		StatusCode import(ImportStack& importStack) override;
	};
} // namespace rive

#endif