#ifndef _RIVE_FILE_HPP_
#define _RIVE_FILE_HPP_

#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/factory.hpp"
#include "rive/file_asset_loader.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include <vector>
#include <set>

///
/// Default namespace for Rive Cpp runtime code.
///
namespace rive
{
class BinaryReader;
class RuntimeHeader;
class Factory;

///
/// Tracks the success/failure result when importing a Rive file.
///
enum class ImportResult
{
    /// Indicates that a file's been successfully imported.
    success,
    /// Indicates that the Rive file is not supported by this runtime.
    unsupportedVersion,
    /// Indicates that the there is a formatting problem in the file itself.
    malformed
};

///
/// A Rive file.
///
class File
{
public:
    /// Major version number supported by the runtime.
    static const int majorVersion = 7;
    /// Minor version number supported by the runtime.
    static const int minorVersion = 0;

    File(Factory*, FileAssetLoader*);

public:
    ~File();

    ///
    /// Imports a Rive file from a binary buffer.
    /// @param data the raw date of the file.
    /// @param result is an optional status result.
    /// @param assetLoader is an optional helper to load assets which
    /// cannot be found in-band.
    /// @returns a pointer to the file, or null on failure.
    static std::unique_ptr<File> import(Span<const uint8_t> data,
                                        Factory*,
                                        ImportResult* result = nullptr,
                                        FileAssetLoader* assetLoader = nullptr);

    /// @returns the file's backboard. All files have exactly one backboard.
    Backboard* backboard() const { return m_backboard; }

    /// @returns the number of artboards in the file.
    size_t artboardCount() const { return m_artboards.size(); }
    std::string artboardNameAt(size_t index) const;

    const std::vector<FileAsset*>& assets() const;

    // Instances
    std::unique_ptr<ArtboardInstance> artboardDefault() const;
    std::unique_ptr<ArtboardInstance> artboardAt(size_t index) const;
    std::unique_ptr<ArtboardInstance> artboardNamed(std::string name) const;

    Artboard* artboard() const;

    /// @returns the named artboard. If no artboard is found with that name,
    /// the null pointer is returned.
    Artboard* artboard(std::string name) const;

    /// @returns the artboard at the specified index, or the nullptr if the
    /// index is out of range.
    Artboard* artboard(size_t index) const;

    /// @returns a view model instance of the view model with the specified name.
    ViewModelInstance* createViewModelInstance(std::string name);

    /// @returns a view model instance attached to the artboard if it exists.
    ViewModelInstance* createViewModelInstance(Artboard* artboard);

    /// @returns a view model instance of the viewModel.
    ViewModelInstance* createViewModelInstance(ViewModel* viewModel);

    /// @returns a view model instance of the viewModel by name and instance name.
    ViewModelInstance* createViewModelInstance(std::string name, std::string instanceName);

    /// @returns a view model instance of the viewModel by their indexes.
    ViewModelInstance* createViewModelInstance(size_t index, size_t instanceIndex);

    ViewModel* viewModel(std::string name);
    ViewModelInstanceListItem* viewModelInstanceListItem(ViewModelInstance* viewModelInstance);
    ViewModelInstanceListItem* viewModelInstanceListItem(ViewModelInstance* viewModelInstance,
                                                         Artboard* artboard);

#ifdef WITH_RIVE_TOOLS
    /// Strips FileAssetContents for FileAssets of given typeKeys.
    /// @param data the raw data of the file.
    /// @param result is an optional status result.
    /// @returns the data buffer of the file with the FileAssetContents objects
    /// stripped out.
    static const std::vector<uint8_t> stripAssets(Span<const uint8_t> data,
                                                  std::set<uint16_t> typeKeys,
                                                  ImportResult* result = nullptr);
#endif

private:
    ImportResult read(BinaryReader&, const RuntimeHeader&);

    /// The file's backboard. All Rive files have a single backboard
    /// where the artboards live.
    Backboard* m_backboard;

    /// We just keep these alive for the life of this File
    std::vector<FileAsset*> m_fileAssets;

    /// List of artboards in the file. Each artboard encapsulates a set of
    /// Rive components and animations.
    std::vector<Artboard*> m_artboards;

    std::vector<ViewModel*> m_ViewModels;
    std::vector<DataEnum*> m_Enums;

    Factory* m_factory;

    /// The helper used to load assets when they're not provided in-band
    /// with the file.
    FileAssetLoader* m_assetLoader;

    void completeViewModelInstance(ViewModelInstance* viewModelInstance);
    ViewModelInstance* copyViewModelInstance(ViewModelInstance* viewModelInstance);
};
} // namespace rive
#endif
