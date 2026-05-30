/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace rive::ore
{

class Context;

class ShaderModule : public rive::gpu::GPUResource,
                     public ENABLE_LITE_RTTI(ShaderModule)
{
public:
    /// Texture-sampler pair from RSTB shader reflection.
    /// Records which texture binding is paired with which sampler binding
    /// in the shader's sampling expressions. Used by the GL backend to bind
    /// sampler objects to the correct texture unit.
    struct TextureSamplerPair
    {
        uint8_t textureGroup;
        uint8_t textureBinding;
        uint8_t samplerGroup;
        uint8_t samplerBinding;
    };

    std::vector<TextureSamplerPair> m_textureSamplerPairs;

    // (group, binding) → per-backend native slot map for this shader's
    // resources. Parsed from the RSTB binding-map sidecar by each
    // backend's `makeShaderModule` (via `applyBindingMapFromDesc`
    // below). Consumed by `Pipeline`, which copies it from its vertex /
    // fragment modules at construction.
    BindingMap m_bindingMap;

#ifdef TRACK_RIVE_SHADER_ID
    uint32_t m_shaderAssetId = 0;
    uint32_t shaderAssetId() const { return m_shaderAssetId; }
#endif

    // Helper for backend `makeShaderModule` paths: parse the binding-map
    // sidecar bytes (`desc.bindingMapBytes`) into `m_bindingMap`. The
    // sidecar is mandatory; runtime backends rely on it to translate
    // `@group/@binding` to native slots.
    void applyBindingMapFromDesc(const ShaderModuleDesc& desc)
    {
        assert(desc.bindingMapBytes != nullptr && desc.bindingMapSize > 0 &&
               "ShaderModuleDesc::bindingMapBytes is required");
        const bool ok = BindingMap::fromBlob(desc.bindingMapBytes,
                                             desc.bindingMapSize,
                                             &m_bindingMap);
        assert(ok && "binding-map sidecar failed to parse");
        (void)ok;
        applyGLFixupFromDesc(desc);
#ifdef TRACK_RIVE_SHADER_ID
        m_shaderAssetId = desc.shaderAssetId;
#endif
    }

    // One entry in the GL program-link fixup table: tells the runtime
    // which GL binding point / texture unit each named uniform should
    // land on, letting `oreGLFixupProgramBindings` call
    // `glUniformBlockBinding` / `glUniform1i` without parsing the
    // emitted GLSL names at runtime.
    struct GLFixupEntry
    {
        enum class Kind : uint8_t
        {
            UBOBlock = 0,
            SamplerUniform = 1,
        };
        Kind kind;
        uint8_t slot;
        std::string name;
    };

    // Parsed fixup table. Populated from the stage's RSTB sidecar (target
    // 14 = VS, target 15 = FS). Empty for non-GL backends, where these
    // sidecars are not part of the variant set.
    // `oreGLFixupProgramBindings` iterates both stages' tables at
    // `glLinkProgram` time.
    std::vector<GLFixupEntry> m_glFixup;

    // Helper: parse `desc.glFixupBytes` (RSTB GL fixup blob format)
    // into `m_glFixup`. No-op when the sidecar is absent or malformed.
    void applyGLFixupFromDesc(const ShaderModuleDesc& desc)
    {
        if (desc.glFixupBytes == nullptr || desc.glFixupSize < 3)
            return;
        const uint8_t* p = desc.glFixupBytes;
        const uint8_t* end = p + desc.glFixupSize;
        if (*p++ != 1) // version
            return;
        uint16_t count = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
        p += 2;
        m_glFixup.reserve(count);
        for (uint16_t i = 0; i < count; ++i)
        {
            if (end - p < 4)
                return;
            GLFixupEntry e;
            e.kind = static_cast<GLFixupEntry::Kind>(*p++);
            e.slot = *p++;
            uint16_t nameLen = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
            p += 2;
            if (end - p < nameLen)
                return;
            e.name.assign(reinterpret_cast<const char*>(p), nameLen);
            p += nameLen;
            m_glFixup.push_back(std::move(e));
        }
    }

    virtual ~ShaderModule() = default;

protected:
    friend class Context;
    friend class Pipeline;

    ShaderModule() : rive::gpu::GPUResource(nullptr) {}
    ShaderModule(rcp<rive::gpu::GPUResourceManager> manager) :
        rive::gpu::GPUResource(std::move(manager))
    {}
};

} // namespace rive::ore
