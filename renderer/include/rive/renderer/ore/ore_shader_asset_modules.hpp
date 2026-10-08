#pragma once

#include "rive/assets/shader_asset.hpp"
#include "rive/renderer/ore/ore_rstb_entry_container.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/simple_array.hpp"

#include <cstring>
#include <vector>

// The one place a ShaderAsset becomes ShaderModuleDescs, so every scripting
// lane builds the same modules from the same admitted bytes.

namespace rive
{
namespace ore
{
// RSTB sidecar target carrying the binding map for a source target.
inline uint8_t bindingMapTargetFor(ShaderTarget target)
{
    switch (target)
    {
        case ShaderTarget::wgsl:
            return 16;
        case ShaderTarget::glsl:
            return 11;
        case ShaderTarget::msl:
            return 10;
        case ShaderTarget::hlsl:
            return 12;
        case ShaderTarget::spirv:
            return 13;
    }
    return 255;
}

// Four bytes per pair in ShaderModuleDesc::texSamplerPairBytes order.
inline std::vector<uint8_t> textureSamplerPairBytes(const ShaderAsset& asset)
{
    std::vector<uint8_t> bytes;
    auto pairs = asset.textureSamplerPairs();
    bytes.reserve(pairs.size() * 4);
    for (const auto& pair : pairs)
    {
        bytes.push_back(pair.texGroup);
        bytes.push_back(pair.texBinding);
        bytes.push_back(pair.sampGroup);
        bytes.push_back(pair.sampBinding);
    }
    return bytes;
}

// Calls visit(desc, first, count, entries) once per module the asset's variant
// for target builds: one per vertex or fragment entry on per entry targets
// (GLSL, HLSL), else one shared by entries [first, first + count). visit
// returns false to stop. False when the variant is missing or malformed.
template <typename Visit>
bool visitShaderAssetModules(ShaderTarget target,
                             const ShaderAsset& asset,
                             Visit&& visit)
{
    auto blob = asset.findShader(static_cast<uint8_t>(target));
    if (blob.empty())
    {
        return false;
    }
    uint8_t bindingMapTarget = bindingMapTargetFor(target);
    auto bindingMap = bindingMapTarget == 255
                          ? Span<const uint8_t>{}
                          : asset.findShader(bindingMapTarget);
    std::vector<uint8_t> pairBytes = textureSamplerPairBytes(asset);

    ShaderModuleDesc desc;
    desc.bindingMapBytes = bindingMap.empty() ? nullptr : bindingMap.data();
    desc.bindingMapSize = static_cast<uint32_t>(bindingMap.size());
    desc.texSamplerPairBytes = pairBytes.empty() ? nullptr : pairBytes.data();
    desc.texSamplerPairSize = static_cast<uint32_t>(pairBytes.size());
    desc.shaderAssetId = asset.assetId();

    std::vector<RstbEntryView> entries;
    const uint32_t blobSize = static_cast<uint32_t>(blob.size());
    if (target == ShaderTarget::glsl || target == ShaderTarget::hlsl)
    {
        if (!parsePerEntryContainer(blob.data(), blobSize, entries))
        {
            return false;
        }
        for (uint32_t i = 0; i < entries.size(); i++)
        {
            const auto& entry = entries[i];
            // These containers only carry vertex and fragment modules.
            if (entry.stage > 1)
            {
                continue;
            }
            ShaderModuleDesc entryDesc = desc;
            entryDesc.stage =
                entry.stage == 0 ? ShaderStage::vertex : ShaderStage::fragment;
            if (target == ShaderTarget::hlsl)
            {
                entryDesc.hlslSource =
                    reinterpret_cast<const char*>(entry.source);
                entryDesc.hlslSourceSize = entry.sourceSize;
                entryDesc.hlslEntryPoint = entry.physical.c_str();
            }
            else
            {
                entryDesc.code = entry.source;
                entryDesc.codeSize = entry.sourceSize;
                auto fixup = asset.findShader(entry.stage == 0 ? 14 : 15);
                entryDesc.glFixupBytes = fixup.empty() ? nullptr : fixup.data();
                entryDesc.glFixupSize = static_cast<uint32_t>(fixup.size());
            }
            if (!visit(entryDesc, i, 1u, entries))
            {
                break;
            }
        }
        return true;
    }

    const uint8_t* source = nullptr;
    uint32_t sourceSize = 0;
    if (!parseWholeModuleContainer(blob.data(),
                                   blobSize,
                                   entries,
                                   &source,
                                   &sourceSize))
    {
        return false;
    }
    desc.code = source;
    desc.codeSize = sourceSize;
    visit(desc, 0u, static_cast<uint32_t>(entries.size()), entries);
    return true;
}

// Decodes a bare RSTB container, such as an editor live compile, which comes
// without the signed content envelope file assets carry.
inline bool decodeBareRstb(ShaderAsset& asset,
                           const uint8_t* rstb,
                           uint32_t size)
{
    if (rstb == nullptr || size == 0)
    {
        return false;
    }
    SimpleArray<uint8_t> envelope(static_cast<size_t>(size) + 1);
    envelope[0] = 0x00;
    memcpy(envelope.data() + 1, rstb, size);
    return asset.decode(envelope, nullptr);
}
} // namespace ore
} // namespace rive
