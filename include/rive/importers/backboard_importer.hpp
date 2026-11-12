#ifndef _RIVE_BACKBOARD_IMPORTER_HPP_
#define _RIVE_BACKBOARD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/refcnt.hpp"
#include "rive/file.hpp"
#include <unordered_map>
#include <vector>

namespace rive
{
class Artboard;
class NestedArtboard;
class Backboard;
class FileAsset;
class FileAssetReferencer;
class DataConverter;
class DataBind;
class DataConverterGroupItem;
class ScriptInputArtboard;
class ScrollPhysics;
class ArtboardReferencer;
class ViewModelInstance;
class BackboardImporter : public ImportStackObject
{
private:
  Backboard* m_Backboard;
  std::unordered_map<int, Artboard*> m_ArtboardLookup;
  std::vector<ArtboardReferencer*> m_ArtboardsReferencers;
  std::vector<rcp<FileAsset>> m_FileAssets;
  std::vector<FileAssetReferencer*> m_FileAssetReferencers;
  std::vector<DataConverter*> m_DataConverters;
  std::vector<DataBind*> m_DataConverterReferencers;
  std::vector<DataConverterGroupItem*> m_DataConverterGroupItemReferencers;
  std::vector<KeyFrameInterpolator*> m_interpolators;
  std::vector<ScrollPhysics*> m_physics;
  int m_NextArtboardId;
  File* m_file = nullptr;
  // Edges of the nesting graph from artboards whose bodies the import
  // deferred, which have no referencers yet: (nesting artboard, id of the
  // artboard it references).
  std::vector<std::pair<Artboard*, int>> m_deferredNestingEdges;
  // While decoding a deferred body: the file's nesting components, which
  // the import computed over the whole graph, in place of components
  // computed from this importer's referencers alone.
  const std::unordered_map<const Artboard*, int>* m_nestingComponents = nullptr;
  // While decoding a deferred body: the file's artboard lookup and assets,
  // read in place (a decode only looks them up) rather than copied for every
  // body.
  const std::unordered_map<int, Artboard*>* m_seededArtboardLookup = nullptr;
  const std::vector<rcp<FileAsset>>* m_seededFileAssets = nullptr;
  const std::unordered_map<int, Artboard*>& artboardLookup() const
  {
    return m_seededArtboardLookup != nullptr ? *m_seededArtboardLookup
                                             : m_ArtboardLookup;
  }
  // The converters are the file's, already bound to their interpolators.
  bool m_convertersSeeded = false;
  // Hand the artboard lookup and the nesting components to the file when
  // resolving, for deferred bodies to resolve against later.
  bool m_keepsResolution = false;
  // Where converter referencers go as this one resolves, when it reads an
  // animation body inside another import of the file.
  BackboardImporter* m_forwardTo = nullptr;

public:
  BackboardImporter(Backboard* backboard);
  void addArtboard(Artboard* artboard);
  void addMissingArtboard();
  void addArtboardReferencer(ArtboardReferencer* artboard);
  void addFileAsset(rcp<FileAsset> asset);
  void addFileAssetReferencer(FileAssetReferencer* referencer);
  void addDataConverterReferencer(DataBind* referencer);
  void addDataConverter(DataConverter* converter);
  // The most recently imported file level converter, used by objects that
  // are written right after the converter they belong to.
  DataConverter* lastDataConverter() const
  {
    return m_DataConverters.empty() ? nullptr : m_DataConverters.back();
  }
  void addDataConverterGroupItemReferencer(DataConverterGroupItem* referencer);
  void addInterpolator(KeyFrameInterpolator* interpolator);

  /// Adds an interpolator that is already live in the file, for a partial
  /// import that only needs to resolve against it. Unlike [addInterpolator]
  /// it does not initialize: the object is in use and re-initializing it
  /// would reset state the rest of the file is depending on.
  void seedInterpolator(KeyFrameInterpolator* interpolator);
  void addPhysics(ScrollPhysics* physics);
  /// Gives [instance] its view model, and lists it there when it's file
  /// level ([listed]).
  void addViewModelInstance(ViewModelInstance* instance, bool listed);

  /// An edge of the nesting graph from an artboard whose body the import
  /// deferred: [nesting]'s body references the artboard with
  /// [referencedArtboardId]. Cycles are found over the whole graph.
  void addDeferredNestingEdge(Artboard* nesting, int referencedArtboardId);
  /// Resolve hands the file this importer's artboard lookup and the nesting
  /// graph's components, for deferred bodies to resolve against.
  void keepResolution() { m_keepsResolution = true; }

  // Seeding, for decoding one deferred body against what the file already
  // holds. Unlike the add* calls these change nothing in what they're given.
  // The lookup and the assets are read in place, so they must outlive the
  // importer.
  void seedArtboardLookup(const std::unordered_map<int, Artboard*>& lookup);
  void seedFileAssets(const std::vector<rcp<FileAsset>>& assets);
  void seedDataConverters(const std::vector<DataConverter*>& converters);
  void nestingComponents(
      const std::unordered_map<const Artboard*, int>* components)
  {
    m_nestingComponents = components;
  }
  /// For a body decoded inside the file's import (a Joystick reads its
  /// animations as its artboard initializes; a reference looks up the
  /// instance it names): as this one resolves, the converter referencers
  /// its binds registered move to [outer], the import's, which resolves them
  /// with everything else it holds, once the file's converters are complete.
  /// This one clones none.
  void forwardConverterReferencersTo(BackboardImporter* outer)
  {
    m_forwardTo = outer;
  }
  std::vector<ScrollPhysics*> physics() { return m_physics; }
  const std::vector<rcp<FileAsset>>* assets() const
  {
    return m_seededFileAssets != nullptr ? m_seededFileAssets : &m_FileAssets;
  }
  void file(File* value);
  File* file() { return m_file; };

  StatusCode resolve() override;
  const Backboard* backboard() const { return m_Backboard; }
};
} // namespace rive
#endif
