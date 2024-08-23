#ifndef _RIVE_BACKBOARD_IMPORTER_HPP_
#define _RIVE_BACKBOARD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
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
class BackboardImporter : public ImportStackObject
{
private:
    Backboard* m_Backboard;
    std::unordered_map<int, Artboard*> m_ArtboardLookup;
    std::vector<NestedArtboard*> m_NestedArtboards;
    std::vector<FileAsset*> m_FileAssets;
    std::vector<FileAssetReferencer*> m_FileAssetReferencers;
    std::vector<DataConverter*> m_DataConverters;
    std::vector<DataBind*> m_DataConverterReferencers;
    std::vector<DataConverterGroupItem*> m_DataConverterGroupItemReferencers;
    int m_NextArtboardId;

public:
    BackboardImporter(Backboard* backboard);
    void addArtboard(Artboard* artboard);
    void addMissingArtboard();
    void addNestedArtboard(NestedArtboard* artboard);
    void addFileAsset(FileAsset* asset);
    void addFileAssetReferencer(FileAssetReferencer* referencer);
    void addDataConverterReferencer(DataBind* referencer);
    void addDataConverter(DataConverter* converter);
    void addDataConverterGroupItemReferencer(DataConverterGroupItem* referencer);

    StatusCode resolve() override;
    const Backboard* backboard() const { return m_Backboard; }
};
} // namespace rive
#endif
