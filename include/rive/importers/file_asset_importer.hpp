#ifndef _RIVE_FILE_ASSET_IMPORTER_HPP_
#define _RIVE_FILE_ASSET_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <unordered_map>
#include <vector>

namespace rive
{
	class FileAsset;
	class FileAssetContents;
	class FileAssetImporter : public ImportStackObject
	{
	private:
		FileAsset* m_FileAsset;

	public:
		FileAssetImporter(FileAsset* fileAsset);
		void loadContents(const FileAssetContents& contents);
		StatusCode resolve() override;
	};
} // namespace rive
#endif
