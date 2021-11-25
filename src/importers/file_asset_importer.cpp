#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"

using namespace rive;

FileAssetImporter::FileAssetImporter(FileAsset* fileAsset) :
    m_FileAsset(fileAsset)
{
}
void FileAssetImporter::loadContents(const FileAssetContents& contents)
{
	const std::vector<uint8_t>& data = contents.bytes();
	m_FileAsset->decode(&data[0], data.size());
}

StatusCode FileAssetImporter::resolve()
{
	// TODO: find data if the bytes weren't in band.
	return StatusCode::Ok;
}