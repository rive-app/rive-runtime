
#include "rive/importers/backboard_importer.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/file_asset.hpp"
#include <unordered_set>

using namespace rive;

BackboardImporter::BackboardImporter(Backboard* backboard) :
    m_Backboard(backboard), m_NextArtboardId(0)
{}
void BackboardImporter::addNestedArtboard(NestedArtboard* artboard)
{
    m_NestedArtboards.push_back(artboard);
}

void BackboardImporter::addFileAsset(FileAsset* asset)
{
    m_FileAssets.push_back(asset);
    {
        // EDITOR BUG 4204
        // --------------
        // Ensure assetIds are unique. Due to an editor bug:
        // https://github.com/rive-app/rive/issues/4204
        std::unordered_set<uint32_t> ids;
        uint32_t nextId = 1;
        for (auto fileAsset : m_FileAssets)
        {
            if (ids.count(fileAsset->assetId()))
            {
                fileAsset->assetId(nextId);
            }
            else
            {
                ids.insert(fileAsset->assetId());
                if (fileAsset->assetId() >= nextId)
                {
                    nextId = fileAsset->assetId() + 1;
                }
            }
        }

        // --------------
    }
}

void BackboardImporter::addFileAssetReferencer(FileAssetReferencer* referencer)
{
    m_FileAssetReferencers.push_back(referencer);
}

void BackboardImporter::addArtboard(Artboard* artboard)
{
    m_ArtboardLookup[m_NextArtboardId++] = artboard;
}

void BackboardImporter::addMissingArtboard() { m_NextArtboardId++; }

StatusCode BackboardImporter::resolve()
{

    for (auto nestedArtboard : m_NestedArtboards)
    {
        auto itr = m_ArtboardLookup.find(nestedArtboard->artboardId());
        if (itr != m_ArtboardLookup.end())
        {
            auto artboard = itr->second;
            if (artboard != nullptr)
            {
                nestedArtboard->nest(artboard);
            }
        }
    }
    for (auto referencer : m_FileAssetReferencers)
    {
        auto index = (size_t)referencer->assetId();
        if (index >= m_FileAssets.size())
        {
            continue;
        }
        auto asset = m_FileAssets[index];
        referencer->setAsset(asset);
    }

    return StatusCode::Ok;
}
