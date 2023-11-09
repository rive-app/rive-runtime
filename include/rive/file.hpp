#ifndef _RIVE_FILE_HPP_
#define _RIVE_FILE_HPP_

#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/factory.hpp"
#include "rive/file_asset_loader.hpp"
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

    Factory* m_factory;

    /// The helper used to load assets when they're not provided in-band
    /// with the file.
    FileAssetLoader* m_assetLoader;
};
} // namespace rive
#endif
