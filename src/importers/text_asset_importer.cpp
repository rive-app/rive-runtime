#ifdef WITH_RIVE_SCRIPTING
#include "rive/importers/text_asset_importer.hpp"
#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/text_asset.hpp"
#include "rive/signed_content_header.hpp"
#include "rive/file_asset_loader.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include "libhydrogen.h"

using namespace rive;

namespace rive
{
#ifdef WITH_RIVE_TEST_SIGNATURE
// Test-only public key matching the sample private key in
// `packages/rive_core/lib/runtime/signing_context.dart::_samplePrivateKey`
// (the last 32 bytes of that 64-byte libhydrogen keypair). Lets a runtime
// build verify .riv files signed locally via `SampleSigningContext`, which
// is useful for end-to-end testing the export+load pipeline without the
// production signing backend. NEVER enable this flag on a shipping build —
// it accepts .rivs any attacker could produce.
const uint8_t g_scriptVerificationPublicKey[32] = {
    180, 113, 86, 235, 225, 24, 110, 236, 105, 86, 201, 6,   73, 5,   203, 102,
    81,  179, 12, 240, 226, 55, 103, 134, 227, 94, 82,  187, 51, 178, 96,  46};
#else
// Public key for in-band content signature verification (32 bytes)
const uint8_t g_scriptVerificationPublicKey[32] = {
    159, 202, 90,  135, 12,  153, 157, 21,  112, 103, 62,
    130, 59,  196, 187, 236, 103, 210, 239, 227, 175, 97,
    222, 254, 70,  53,  212, 18,  191, 143, 101, 108};
#endif
} // namespace rive

TextAssetImporter::TextAssetImporter(
    TextAsset* fileAsset,
    rcp<FileAssetLoader> assetLoader,
    Factory* factory,
    std::vector<InBandContent>* verificationSet) :
    FileAssetImporter(fileAsset, assetLoader, factory),
    m_verificationSet(verificationSet)
{}

TextAsset* TextAssetImporter::textAsset()
{
    return m_fileAsset->as<TextAsset>();
}

void TextAssetImporter::onFileAssetContents(
    std::unique_ptr<FileAssetContents> contents)
{
    // When contents are found in-band, this asset's raw bytes become part of
    // the aggregate verification set. Strip the SignedContentHeader first.
    auto& bytes = contents->bytes();
    SignedContentHeader header(Span<const uint8_t>(bytes.data(), bytes.size()));
    if (header.isValid())
    {
        auto content = header.content();
        SimpleArray<uint8_t> rawContent(content.data(), content.size());
        m_verificationSet->emplace_back(InBandContent(textAsset(), rawContent));
    }
    FileAssetImporter::onFileAssetContents(std::move(contents));
}

InBandContent::InBandContent(TextAsset* asset, SimpleArray<uint8_t>& content) :
    m_textAsset(asset), m_bytes(SimpleArray<uint8_t>(content))
{}

StatusCode TextAssetImporter::resolve()
{
    auto status = FileAssetImporter::resolve();
    if (status != StatusCode::Ok)
    {
        return status;
    }

    if (m_content && !m_content->signature().empty())
    {
        std::vector<uint8_t> combinedBytecode;
        // Concatenate every in-band payload captured so far.
        for (auto& inband : *m_verificationSet)
        {
            SimpleArray<uint8_t>& bytecode = inband.m_bytes;
            combinedBytecode.insert(combinedBytecode.end(),
                                    bytecode.begin(),
                                    bytecode.end());
        }

        Span<uint8_t> signature = m_content->signature();
        if (signature.size() != hydro_sign_BYTES)
        {
            return StatusCode::Ok;
        }
        int isVerified = hydro_sign_verify(signature.data(),
                                           combinedBytecode.data(),
                                           combinedBytecode.size(),
                                           "RiveCode",
                                           g_scriptVerificationPublicKey);

        // Propagate the aggregate verification result to every participating
        // TextAsset (Luau modules and RSTB blobs alike).
        for (auto& inband : *m_verificationSet)
        {
            inband.m_textAsset->m_verified = isVerified == 0;
        }
        m_verificationSet->clear();
    }

    // Note that it's ok for an asset to not resolve (or to resolve async).
    return StatusCode::Ok;
}

#endif
