#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_TEXT_ASSET_IMPORTER_HPP_
#define _RIVE_TEXT_ASSET_IMPORTER_HPP_

#include "rive/importers/file_asset_importer.hpp"
#include "rive/simple_array.hpp"
#include <vector>

namespace rive
{
class TextAsset;
class TextAssetImporter;

/// A single in-band payload (Luau bytecode or RSTB blob) captured during
/// import, held until the aggregate signature is verified in resolve().
class InBandContent
{
    friend class TextAssetImporter;

public:
    InBandContent(TextAsset* asset, SimpleArray<uint8_t>& bytes);

private:
    TextAsset* m_textAsset;
    SimpleArray<uint8_t> m_bytes;
};

/// Importer shared by all TextAsset subclasses (ScriptAsset, ShaderAsset).
/// Strips the SignedContentHeader envelope from in-band FileAssetContents and
/// contributes the raw content bytes to a shared verification set. The last
/// importer with a signature runs hydro_sign_verify against the concatenated
/// bytes and marks every participating asset as verified.
class TextAssetImporter : public FileAssetImporter
{
public:
    TextAssetImporter(TextAsset*,
                      rcp<FileAssetLoader>,
                      Factory*,
                      std::vector<InBandContent>*);
    StatusCode resolve() override;
    void onFileAssetContents(
        std::unique_ptr<FileAssetContents> contents) override;

    TextAsset* textAsset();

private:
    // The set of in-band contents which are signed together.
    std::vector<InBandContent>* m_verificationSet;
};

// Public key for in-band content signature verification (32 bytes)
// TODO: Replace with permanent production public key.
extern const uint8_t g_scriptVerificationPublicKey[32];
} // namespace rive
#endif
#endif
