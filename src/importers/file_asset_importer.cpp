#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/file_asset_resolver.hpp"

using namespace rive;

FileAssetImporter::FileAssetImporter(FileAsset* fileAsset,
                                     FileAssetResolver* assetResolver) :
    m_FileAsset(fileAsset), m_FileAssetResolver(assetResolver)
{
}

void FileAssetImporter::loadContents(const FileAssetContents& contents)
{
	const std::vector<uint8_t>& data = contents.bytes();
	if (m_FileAsset->decode(&data[0], data.size()))
	{
		m_LoadedContents = true;
	}
}

StatusCode FileAssetImporter::resolve()
{
	if (!m_LoadedContents && m_FileAssetResolver != nullptr)
	{
		// Contents weren't available in-band, or they couldn't be decoded. Try
		// to find them out of band.
		m_FileAssetResolver->loadContents(*m_FileAsset);
	}

	// Note that it's ok for an asset to not resolve (or to resolve async).
	return StatusCode::Ok;
}