
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
#include "rive/file.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/script_input_artboard.hpp"
#include <algorithm>
#include <unordered_set>
#include <utility>

using namespace rive;

namespace
{
// Flags each referencer whose reference lies on a nested artboard cycle, which
// would make instancing recurse without end. The editor never exports those.
std::vector<bool> closesNestedArtboardCycle(
    const std::vector<ArtboardReferencer*>& referencers,
    const std::unordered_map<int, Artboard*>& artboards)
{
    std::unordered_map<Artboard*, int> nodes;
    auto node = [&](Artboard* artboard) {
        return nodes.emplace(artboard, (int)nodes.size()).first->second;
    };
    std::vector<std::pair<int, int>> references(referencers.size(), {-1, -1});
    std::vector<std::vector<int>> edges;
    for (size_t i = 0; i < referencers.size(); i++)
    {
        Artboard* nesting = referencers[i]->nestingArtboard();
        auto nested = artboards.find(referencers[i]->referencedArtboardId());
        if (nesting == nullptr || nested == artboards.end() ||
            nested->second == nullptr)
        {
            continue;
        }
        references[i] = {node(nesting), node(nested->second)};
        edges.resize(nodes.size());
        edges[references[i].first].push_back(references[i].second);
    }

    // Iterative Tarjan: a reference closes a cycle when both its ends share a
    // strongly connected component. Recursing could overflow on large files.
    const size_t count = edges.size();
    std::vector<int> order(count, -1);
    std::vector<int> lowLink(count, 0);
    std::vector<int> component(count, -1);
    std::vector<int> open;
    std::vector<std::pair<int, size_t>> calls;
    int nextOrder = 0;
    int nextComponent = 0;
    for (int root = 0; root < (int)count; root++)
    {
        if (order[root] != -1)
        {
            continue;
        }
        order[root] = lowLink[root] = nextOrder++;
        open.push_back(root);
        calls.push_back({root, 0});
        while (!calls.empty())
        {
            int current = calls.back().first;
            size_t next = calls.back().second++;
            if (next < edges[current].size())
            {
                int to = edges[current][next];
                if (order[to] == -1)
                {
                    order[to] = lowLink[to] = nextOrder++;
                    open.push_back(to);
                    calls.push_back({to, 0});
                }
                else if (component[to] == -1)
                {
                    lowLink[current] = std::min(lowLink[current], order[to]);
                }
                continue;
            }
            calls.pop_back();
            if (!calls.empty())
            {
                int parent = calls.back().first;
                lowLink[parent] = std::min(lowLink[parent], lowLink[current]);
            }
            if (lowLink[current] == order[current])
            {
                int member;
                do
                {
                    member = open.back();
                    open.pop_back();
                    component[member] = nextComponent;
                } while (member != current);
                nextComponent++;
            }
        }
    }

    std::vector<bool> closesCycle(referencers.size(), false);
    for (size_t i = 0; i < referencers.size(); i++)
    {
        closesCycle[i] =
            references[i].first != -1 &&
            component[references[i].first] == component[references[i].second];
    }
    return closesCycle;
}
} // namespace

BackboardImporter::BackboardImporter(Backboard* backboard) :
    m_Backboard(backboard), m_NextArtboardId(0)
{}
void BackboardImporter::addArtboardReferencer(ArtboardReferencer* artboard)
{
    m_ArtboardsReferencers.push_back(artboard);
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
    auto closesCycle =
        closesNestedArtboardCycle(m_ArtboardsReferencers, m_ArtboardLookup);
    for (size_t i = 0; i < m_ArtboardsReferencers.size(); i++)
    {
        if (closesCycle[i])
        {
            continue;
        }
        auto nestedArtboard = m_ArtboardsReferencers[i];
        auto itr =
            m_ArtboardLookup.find(nestedArtboard->referencedArtboardId());
        if (itr != m_ArtboardLookup.end())
        {
            auto artboard = itr->second;
            if (artboard != nullptr)
            {
                nestedArtboard->referencedArtboard(artboard);
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

void BackboardImporter::seedInterpolator(KeyFrameInterpolator* interpolator)
{
    m_interpolators.push_back(interpolator);
}

void BackboardImporter::addPhysics(ScrollPhysics* physics)
{
    m_physics.push_back(physics);
}

void BackboardImporter::addViewModelInstance(ViewModelInstance* instance)
{
    if (m_file == nullptr)
    {
        return;
    }
    auto viewModel = m_file->viewModel((size_t)instance->viewModelId());
    if (viewModel != nullptr)
    {
        viewModel->addInstance(instance);
    }
}

void BackboardImporter::file(File* value) { m_file = value; }