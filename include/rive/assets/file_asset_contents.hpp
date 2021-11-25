#ifndef _RIVE_FILE_ASSET_CONTENTS_HPP_
#define _RIVE_FILE_ASSET_CONTENTS_HPP_
#include "rive/generated/assets/file_asset_contents_base.hpp"

namespace rive
{
	class FileAssetContents : public FileAssetContentsBase
	{
	public:
		StatusCode import(ImportStack& importStack) override;
	};
} // namespace rive

#endif