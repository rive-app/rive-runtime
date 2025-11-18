#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_SCRIPT_ASSET_IMPORTER_HPP_
#define _RIVE_SCRIPT_ASSET_IMPORTER_HPP_

#include "rive/importers/file_asset_importer.hpp"
#include "rive/simple_array.hpp"
#include <vector>

namespace rive
{
class ScriptAsset;
class ScriptAssetImporter;

class InBandByteCode
{
    friend class ScriptAssetImporter;

public:
    InBandByteCode(ScriptAsset* asset, SimpleArray<uint8_t>& bytes);

private:
    ScriptAsset* m_scriptAsset;
    SimpleArray<uint8_t> m_bytes;
};

class ScriptAssetImporter : public FileAssetImporter
{
public:
    ScriptAssetImporter(ScriptAsset*,
                        rcp<FileAssetLoader>,
                        Factory*,
                        std::vector<InBandByteCode>*);
    StatusCode resolve() override;
    void onFileAssetContents(
        std::unique_ptr<FileAssetContents> contents) override;

    ScriptAsset* scriptAsset();

private:
    // The set of scripts which are signed together.
    std::vector<InBandByteCode>* m_scriptVerificationSet;
};

// Public key for script signature verification (32 bytes)
// TODO: Replace with permanent production public key.
extern const uint8_t g_scriptVerificationPublicKey[32];
} // namespace rive
#endif
#endif