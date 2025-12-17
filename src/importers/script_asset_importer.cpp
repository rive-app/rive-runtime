#ifdef WITH_RIVE_SCRIPTING
#include "rive/importers/script_asset_importer.hpp"
#include "rive/importers/file_asset_importer.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/bytecode_header.hpp"
#include "rive/file_asset_loader.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include "libhydrogen.h"

using namespace rive;

namespace rive
{
// Public key for script signature verification (32 bytes)
// TODO: Replace with permanent production public key.
const uint8_t g_scriptVerificationPublicKey[32] = {
    180, 113, 86, 235, 225, 24, 110, 236, 105, 86, 201, 6,   73, 5,   203, 102,
    81,  179, 12, 240, 226, 55, 103, 134, 227, 94, 82,  187, 51, 178, 96,  46};
} // namespace rive

ScriptAssetImporter::ScriptAssetImporter(
    ScriptAsset* fileAsset,
    rcp<FileAssetLoader> assetLoader,
    Factory* factory,
    std::vector<InBandByteCode>* inbandBytecode) :
    FileAssetImporter(fileAsset, assetLoader, factory),
    m_scriptVerificationSet(inbandBytecode)
{}

ScriptAsset* ScriptAssetImporter::scriptAsset()
{
    return m_fileAsset->as<ScriptAsset>();
}

void ScriptAssetImporter::onFileAssetContents(
    std::unique_ptr<FileAssetContents> contents)
{
    // When contents are found in band, this script is part of the verification
    // set. Strip the header to get raw bytecode for aggregate verification.
    auto& bytes = contents->bytes();
    BytecodeHeader header(Span<const uint8_t>(bytes.data(), bytes.size()));
    if (header.isValid())
    {
        auto bytecode = header.bytecode();
        SimpleArray<uint8_t> rawBytecode(bytecode.data(), bytecode.size());
        m_scriptVerificationSet->emplace_back(
            InBandByteCode(scriptAsset(), rawBytecode));
    }
    FileAssetImporter::onFileAssetContents(std::move(contents));
}

InBandByteCode::InBandByteCode(ScriptAsset* asset,
                               SimpleArray<uint8_t>& content) :
    m_scriptAsset(asset), m_bytes(SimpleArray<uint8_t>(content))
{}

StatusCode ScriptAssetImporter::resolve()
{
    auto status = FileAssetImporter::resolve();
    if (status != StatusCode::Ok)
    {
        return status;
    }

    if (m_content && !m_content->signature().empty())
    {
        std::vector<uint8_t> combinedBytecode;
        // Verify the scripts loaded so far which have content in-band.
        for (auto& inband : *m_scriptVerificationSet)
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

        // Mark all the in-band scripts from verifyFrom onwards.
        for (auto& inband : *m_scriptVerificationSet)
        {
            inband.m_scriptAsset->m_verified = isVerified == 0;
        }
        m_scriptVerificationSet->clear();
    }

    // Note that it's ok for an asset to not resolve (or to resolve async).
    return StatusCode::Ok;
}

#endif