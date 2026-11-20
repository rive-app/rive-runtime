#include <array>

#include "rive/assets/file_asset.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

StatusCode FileAsset::import(ImportStack& importStack)
{
  if (addsToBackboard())
  {
    auto backboardImporter = importStack.latest<BackboardImporter>(
        Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
      return StatusCode::MissingObject;
    }
    backboardImporter->addFileAsset(ref_rcp(this));
  }

  return Super::import(importStack);
}

std::mutex& FileAsset::referencersMutex()
{
  // Never destroyed: files released during exit still remove their
  // referencers after function statics would have been torn down.
  static std::mutex* mutex = new std::mutex();
  return *mutex;
}

void FileAsset::addFileAssetReferencer(FileAssetReferencer* referencer)
{
  std::lock_guard<std::mutex> lock(referencersMutex());
  m_fileAssetReferencers.push_back(referencer);
}

void FileAsset::removeFileAssetReferencer(FileAssetReferencer* referencer)
{
  std::lock_guard<std::mutex> lock(referencersMutex());
  auto itr = m_fileAssetReferencers.begin();
  while (itr != m_fileAssetReferencers.end())
  {
    if (*itr == referencer)
    {
      itr = m_fileAssetReferencers.erase(itr);
    }
    else
    {
      itr++;
    }
  }
}

std::string FileAsset::uniqueName() const
{
  // remove final extension
  std::string uniqueName = name();
  std::size_t finalDot = uniqueName.rfind('.');

  if (finalDot != std::string::npos)
  {
    uniqueName = uniqueName.substr(0, finalDot);
  }
  return uniqueName + "-" + std::to_string(assetId());
}

std::string FileAsset::uniqueFilename() const
{
  return uniqueName() + "." + fileExtension();
}

void FileAsset::copyCdnUuid(const FileAssetBase& object)
{
  // Should never be called.
  assert(false);
}

void FileAsset::decodeCdnUuid(Span<const uint8_t> value)
{
  m_cdnUuid = std::vector<uint8_t>(value.begin(), value.end());
}

Span<const uint8_t> FileAsset::cdnUuid() const { return m_cdnUuid; }

std::string FileAsset::cdnUuidStr() const
{
  constexpr uint8_t uuidSize = 16;
  if (m_cdnUuid.size() != uuidSize)
  {
    return "";
  }
  const std::array<const int, uuidSize>
      indices{3, 2, 1, 0, 5, 4, 7, 6, 9, 8, 15, 14, 13, 12, 11, 10};

  std::string uuid;
  for (int idx : indices)
  {
    uuid += "0123456789abcdef"[m_cdnUuid[idx] >> 4];
    uuid += "0123456789abcdef"[m_cdnUuid[idx] & 0xF];
    if (idx == 0 || idx == 4 || idx == 6 || idx == 8)
      uuid += '-';
  }

  return uuid;
}