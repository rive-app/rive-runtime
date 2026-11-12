
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
#include "rive/data_bind/converters/data_converter_group.hpp"
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
// Iterative Tarjan: the strongly connected component of each node. Components
// complete in reverse topological order, so an edge between two components
// always runs from the higher number to the lower one. Recursing could
// overflow on large files.
std::vector<int> stronglyConnectedComponents(
    const std::vector<std::vector<int>>& edges)
{
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
  return component;
}

// The strongly connected components of the nesting graph: an edge from each
// referencer's nesting artboard to the artboard it references, plus the
// edges of bodies the import deferred. Only artboards on an edge get one. A
// reference closes a cycle, which would make instancing recurse without end,
// when both its ends share a component. The editor never exports those.
std::unordered_map<const Artboard*, int> computeNestingComponents(
    const std::vector<ArtboardReferencer*>& referencers,
    const std::vector<std::pair<Artboard*, int>>& deferredEdges,
    const std::unordered_map<int, Artboard*>& artboards)
{
  std::vector<Artboard*> artboardOf;
  std::unordered_map<Artboard*, int> nodes;
  auto node = [&](Artboard* artboard) {
    auto inserted = nodes.emplace(artboard, (int)nodes.size());
    if (inserted.second)
    {
      artboardOf.push_back(artboard);
    }
    return inserted.first->second;
  };
  std::vector<std::vector<int>> edges;
  auto addEdge = [&](Artboard* nesting, int referencedArtboardId) {
    auto nested = artboards.find(referencedArtboardId);
    if (nesting == nullptr || nested == artboards.end() ||
        nested->second == nullptr)
    {
      return;
    }
    int from = node(nesting);
    int to = node(nested->second);
    edges.resize(nodes.size());
    edges[from].push_back(to);
  };
  for (auto referencer : referencers)
  {
    addEdge(referencer->nestingArtboard(), referencer->referencedArtboardId());
  }
  for (auto& edge : deferredEdges)
  {
    addEdge(edge.first, edge.second);
  }

  auto component = stronglyConnectedComponents(edges);
  std::unordered_map<const Artboard*, int> components;
  for (size_t i = 0; i < component.size(); i++)
  {
    components[artboardOf[i]] = component[i];
  }
  return components;
}

// Flags each referencer whose reference lies on a nesting cycle: both its
// ends share a component.
std::vector<bool> closesNestedArtboardCycle(
    const std::vector<ArtboardReferencer*>& referencers,
    const std::unordered_map<int, Artboard*>& artboards,
    const std::unordered_map<const Artboard*, int>& components)
{
  std::vector<bool> closesCycle(referencers.size(), false);
  for (size_t i = 0; i < referencers.size(); i++)
  {
    auto nesting = components.find(referencers[i]->nestingArtboard());
    auto nested = artboards.find(referencers[i]->referencedArtboardId());
    if (nesting == components.end() || nested == artboards.end())
    {
      continue;
    }
    auto nestedComponent = components.find(nested->second);
    closesCycle[i] = nestedComponent != components.end() &&
                     nestedComponent->second == nesting->second;
  }
  return closesCycle;
}

// Gives each bind on a file level converter (on its properties, formula tokens,
// template arguments or script inputs) a clone of the converter it uses. Every
// bind that uses a converter clones it, so a converter's own binds have to
// resolve before anything clones the converter. Binds whose converter leads
// back to the converter they sit on are left without one: cloning it would
// recurse without end. The editor never exports those. Returns the binds it
// handled.
std::unordered_set<DataBind*> resolveConverterDataBinds(
    const std::vector<DataConverter*>& converters)
{
  const size_t count = converters.size();
  std::unordered_map<DataConverter*, int> indices;
  for (size_t i = 0; i < count; i++)
  {
    indices.emplace(converters[i], (int)i);
  }
  auto converterIndex = [&](DataBind* dataBind) {
    auto index = (size_t)dataBind->converterId();
    return index < count ? (int)index : -1;
  };
  std::vector<std::vector<int>> edges(count);
  for (size_t i = 0; i < count; i++)
  {
    for (auto dataBind : converters[i]->dataBinds())
    {
      int to = converterIndex(dataBind);
      if (to != -1)
      {
        edges[i].push_back(to);
      }
    }
    // A group clones the converters of its items, which brings their binds.
    if (converters[i]->is<DataConverterGroup>())
    {
      for (auto item : converters[i]->as<DataConverterGroup>()->items())
      {
        auto itr = indices.find(item->converter());
        if (itr != indices.end())
        {
          edges[i].push_back(itr->second);
        }
      }
    }
  }

  // Lower components never use higher ones, so resolving in component order
  // finishes every converter before a bind clones it.
  auto component = stronglyConnectedComponents(edges);
  std::vector<int> order(count);
  for (size_t i = 0; i < count; i++)
  {
    order[i] = (int)i;
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    return component[a] < component[b];
  });
  std::unordered_set<DataBind*> resolved;
  for (int i : order)
  {
    for (auto dataBind : converters[i]->dataBinds())
    {
      resolved.insert(dataBind);
      int to = converterIndex(dataBind);
      // Already set when a partial import re-resolves the file's converters.
      if (to == -1 || component[to] == component[i] ||
          dataBind->converter() != nullptr)
      {
        continue;
      }
      dataBind->converter(converters[to]->clone()->as<DataConverter>());
    }
  }
  return resolved;
}
} // namespace

BackboardImporter::BackboardImporter(Backboard* backboard)
    : m_Backboard(backboard)
    , m_NextArtboardId(0)
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

void BackboardImporter::addDeferredNestingEdge(
    Artboard* nesting,
    int referencedArtboardId)
{
  m_deferredNestingEdges.push_back({nesting, referencedArtboardId});
}

void BackboardImporter::seedArtboardLookup(
    const std::unordered_map<int, Artboard*>& lookup)
{
  // Outlives this importer: the file's, or the decode's own.
  m_seededArtboardLookup = &lookup;
}

void BackboardImporter::seedFileAssets(
    const std::vector<rcp<FileAsset>>& assets)
{
  // Their ids were made unique when the file imported them (addFileAsset).
  // Outlives this importer, as the lookup does.
  m_seededFileAssets = &assets;
}

void BackboardImporter::seedDataConverters(
    const std::vector<DataConverter*>& converters)
{
  m_DataConverters = converters;
  m_convertersSeeded = true;
}

StatusCode BackboardImporter::resolve()
{
  if (m_forwardTo != nullptr)
  {
    for (auto referencer : m_DataConverterReferencers)
    {
      m_forwardTo->addDataConverterReferencer(referencer);
    }
    m_DataConverterReferencers.clear();
  }
  std::unordered_map<const Artboard*, int> computed;
  if (m_nestingComponents == nullptr)
  {
    computed = computeNestingComponents(
        m_ArtboardsReferencers,
        m_deferredNestingEdges,
        artboardLookup());
  }
  const auto& components = m_nestingComponents != nullptr ? *m_nestingComponents
                                                          : computed;
  auto closesCycle = closesNestedArtboardCycle(
      m_ArtboardsReferencers,
      artboardLookup(),
      components);
  for (size_t i = 0; i < m_ArtboardsReferencers.size(); i++)
  {
    if (closesCycle[i])
    {
      continue;
    }
    auto nestedArtboard = m_ArtboardsReferencers[i];
    auto itr = artboardLookup().find(nestedArtboard->referencedArtboardId());
    if (itr != artboardLookup().end())
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
    const std::vector<rcp<FileAsset>>& fileAssets = *assets();
    if (index >= fileAssets.size())
    {
      continue;
    }
    auto asset = fileAssets[index];
    referencer->setAsset(asset);
  }

  // Seeded converters were bound when the file imported them; binding them
  // again would only rewrite what other threads may be reading.
  for (auto converter : m_DataConverters)
  {
    if (m_convertersSeeded)
    {
      break;
    }
    if (converter->is<DataConverterRangeMapper>())
    {
      size_t converterId = converter->as<DataConverterRangeMapper>()
                               ->interpolatorId();
      if (converterId != -1 && converterId < m_interpolators.size())
      {
        converter->as<DataConverterRangeMapper>()->interpolator(
            m_interpolators[converterId]);
      }
    }
    else if (converter->is<DataConverterInterpolator>())
    {
      size_t converterId = converter->as<DataConverterInterpolator>()
                               ->interpolatorId();
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
  // Seeded converters' own binds resolved when the file imported them too.
  std::unordered_set<DataBind*> converterDataBinds;
  if (!m_convertersSeeded)
  {
    converterDataBinds = resolveConverterDataBinds(m_DataConverters);
  }
  for (auto referencer : m_DataConverterReferencers)
  {
    if (converterDataBinds.count(referencer) != 0)
    {
      continue;
    }
    auto index = (size_t)referencer->converterId();
    if (index >= m_DataConverters.size() || index < 0)
    {
      continue;
    }
    referencer->converter(
        m_DataConverters[index]->clone()->as<DataConverter>());
  }

  if (m_keepsResolution && m_file != nullptr)
  {
    m_file->keepArtboardResolution(m_ArtboardLookup, std::move(computed));
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

void BackboardImporter::addViewModelInstance(
    ViewModelInstance* instance,
    bool listed)
{
  if (m_file == nullptr)
  {
    return;
  }
  auto viewModel = m_file->viewModel((size_t)instance->viewModelId());
  if (viewModel == nullptr)
  {
    return;
  }
  if (listed)
  {
    viewModel->addInstance(instance);
  }
  else
  {
    instance->viewModel(viewModel);
  }
}

void BackboardImporter::file(File* value) { m_file = value; }