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
    File* m_file;

public:
    BackboardImporter(Backboard* backboard);
    void addArtboard(Artboard* artboard);
    void addMissingArtboard();
    void addArtboardReferencer(ArtboardReferencer* artboard);
    void addFileAsset(rcp<FileAsset> asset);
    void addFileAssetReferencer(FileAssetReferencer* referencer);
    void addDataConverterReferencer(DataBind* referencer);
    void addDataConverter(DataConverter* converter);
    void addDataConverterGroupItemReferencer(
        DataConverterGroupItem* referencer);
    void addInterpolator(KeyFrameInterpolator* interpolator);
    void addPhysics(ScrollPhysics* physics);
    std::vector<ScrollPhysics*> physics() { return m_physics; }
    std::vector<rcp<FileAsset>>* assets() { return &m_FileAssets; }
    void file(File* value);
    File* file() { return m_file; };

    StatusCode resolve() override;
    const Backboard* backboard() const { return m_Backboard; }
};
} // namespace rive
#endif
