#ifndef _RIVE_FILE_ASSET_IMPORTER_HPP_
#define _RIVE_FILE_ASSET_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <unordered_map>
#include <vector>

namespace rive
{
	class FileAsset;
	class FileAssetContents;
	class FileAssetResolver;
	class FileAssetImporter : public ImportStackObject
	{
	private:
		bool m_LoadedContents = false;
		FileAsset* m_FileAsset;
		FileAssetResolver* m_FileAssetResolver;

	public:
		FileAssetImporter(FileAsset* fileAsset,
		                  FileAssetResolver* assetResolver);
		void loadContents(const FileAssetContents& contents);
		StatusCode resolve() override;
	};
} // namespace rive
#endif
