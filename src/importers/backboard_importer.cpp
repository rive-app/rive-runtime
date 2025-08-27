
#include "rive/importers/backboard_importer.hpp"
#include "rive/artboard.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/file.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <unordered_set>

using namespace rive;

BackboardImporter::BackboardImporter(Backboard* backboard) :
    m_Backboard(backboard), m_NextArtboardId(0)
{}
void BackboardImporter::addNestedArtboard(NestedArtboard* artboard)
{
    m_NestedArtboards.push_back(artboard);
}

void BackboardImporter::addFileAsset(rcp<FileAsset> asset)
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
#ifdef WITH_RIVE_TOOLS
    artboard->artboardId((uint16_t)m_NextArtboardId);
#endif
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

    for (auto converter : m_DataConverters)
    {
        if (converter->is<DataConverterRangeMapper>())
        {
            size_t converterId =
                converter->as<DataConverterRangeMapper>()->interpolatorId();
            if (converterId != -1 && converterId < m_interpolators.size())
            {
                converter->as<DataConverterRangeMapper>()->interpolator(
                    m_interpolators[converterId]);
            }
        }
        else if (converter->is<DataConverterInterpolator>())
        {
            size_t converterId =
                converter->as<DataConverterInterpolator>()->interpolatorId();
            if (converterId != -1 && converterId < m_interpolators.size())
            {
                converter->as<DataConverterInterpolator>()->interpolator(
                    m_interpolators[converterId]);
            }
        }
    }
    for (auto referencer : m_DataConverterGroupItemReferencers)
    {
        auto index = (size_t)referencer->converterId();
        if (index >= m_DataConverters.size() || index < 0)
        {
            continue;
        }
        // Do not clone converters at this point because some of them are
        // incomplete
        referencer->converter(m_DataConverters[index]);
    }
    for (auto referencer : m_DataConverterReferencers)
    {
        auto index = (size_t)referencer->converterId();
        if (index >= m_DataConverters.size() || index < 0)
        {
            continue;
        }
        referencer->converter(
            m_DataConverters[index]->clone()->as<DataConverter>());
    }

    return StatusCode::Ok;
}

void BackboardImporter::addDataConverter(DataConverter* dataConverter)
{
    m_DataConverters.push_back(dataConverter);
}

void BackboardImporter::addDataConverterReferencer(DataBind* dataBind)
{
    m_DataConverterReferencers.push_back(dataBind);
}

void BackboardImporter::addDataConverterGroupItemReferencer(
    DataConverterGroupItem* dataBind)
{
    m_DataConverterGroupItemReferencers.push_back(dataBind);
}

void BackboardImporter::addInterpolator(KeyFrameInterpolator* interpolator)
{
    // Since these interpolators do not belong to an artboard, we have to
    // initialize them
    interpolator->initialize();
    m_interpolators.push_back(interpolator);
}

void BackboardImporter::addPhysics(ScrollPhysics* physics)
{
    m_physics.push_back(physics);
}

void BackboardImporter::file(File* value) { m_file = value; }