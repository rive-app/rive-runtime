#ifndef _RIVE_FILE_ASSET_HPP_
#define _RIVE_FILE_ASSET_HPP_
#include "rive/generated/assets/file_asset_base.hpp"
#include "rive/span.hpp"
#include "rive/refcnt.hpp"
#include "rive/simple_array.hpp"
#include <mutex>
#include <string>

namespace rive
{
class Factory;
class FileAssetReferencer;
class FileAsset : public FileAssetBase, public RefCnt<FileAsset>
{
#ifdef WITH_RIVE_SCRIPTING
  friend class TextAssetImporter;
#endif

private:
  std::vector<uint8_t> m_cdnUuid;
  std::vector<FileAssetReferencer*> m_fileAssetReferencers;
  // Guards every asset's referencer list. Instancing an artboard adds a
  // referencer to the source's asset for each image, font and script it
  // clones (and destroying it removes them), and instances of one file can
  // be made and destroyed on several threads at once.
  static std::mutex& referencersMutex();

protected:
  virtual bool addsToBackboard() { return true; }
#ifdef WITH_RIVE_SCRIPTING
  bool m_verified = false;

public:
  /// True once the aggregate in-band signature has verified against this
  /// asset's content. Defaults to false until TextAssetImporter::resolve
  /// runs. Production (non-tools) consumers should gate use of the content
  /// behind this check.
  bool verified() const { return m_verified; }
#endif

public:
#ifdef WITH_RIVE_EDITOR
  void editorRelease() override { unref(); }
#endif
  Span<const uint8_t> cdnUuid() const;
  std::string cdnUuidStr() const;

  void decodeCdnUuid(Span<const uint8_t> value) override;
  void copyCdnUuid(const FileAssetBase& object) override;
  virtual bool decode(SimpleArray<uint8_t>&, Factory*) = 0;
  virtual std::string fileExtension() const = 0;
  StatusCode import(ImportStack& importStack) override;
  /// A copy of the referencers, as the list can change on another thread.
  std::vector<FileAssetReferencer*> fileAssetReferencers() const
  {
    std::lock_guard<std::mutex> lock(referencersMutex());
    return m_fileAssetReferencers;
  }

  /// Calls [fn] with each referencer, holding the lock that guards the list
  /// so an instance made or destroyed on another thread can't change it
  /// mid-walk. [fn] must not add or remove referencers.
  template <typename Fn> void forEachFileAssetReferencer(Fn&& fn)
  {
    std::lock_guard<std::mutex> lock(referencersMutex());
    for (FileAssetReferencer* referencer : m_fileAssetReferencers)
    {
      fn(referencer);
    }
  }

  void addFileAssetReferencer(FileAssetReferencer* referencer);
  void removeFileAssetReferencer(FileAssetReferencer* referencer);

  std::string uniqueName() const;
  std::string uniqueFilename() const;
};
} // namespace rive

#endif