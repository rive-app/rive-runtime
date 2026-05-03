/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#if defined(ORE_BACKEND_METAL)
#import <Metal/Metal.h>
#endif
// Note: load_gles_extensions.hpp (glad) is intentionally NOT included here.
// GL object handles are GLuint = unsigned int; no glad needed in the header.
#if defined(ORE_BACKEND_D3D11)
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <mutex>
#include <vector>
#include <string>
#endif
#if defined(ORE_BACKEND_D3D12)
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#endif
#if defined(ORE_BACKEND_WGPU)
#include <webgpu/webgpu_cpp.h>
#endif
#if defined(ORE_BACKEND_VK)
#include <vulkan/vulkan.h>
#endif

namespace rive::ore
{

class Context;

class ShaderModule : public RefCnt<ShaderModule>
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

    // Helper for backend `makeShaderModule` paths: parse the binding-map
    // sidecar bytes (`desc.bindingMapBytes`) into `m_bindingMap`. The
    // sidecar is mandatory; runtime backends rely on it to translate
    // `@group/@binding` to native slots (RFC §14.4).
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

private:
    friend class Context;
    friend class Pipeline;

    ShaderModule() = default;

#if defined(ORE_BACKEND_METAL)
    id<MTLLibrary> m_mtlLibrary = nil;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glShader = 0;
    unsigned int m_glShaderType = 0; // GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
#endif
#if defined(ORE_BACKEND_D3D11)
    std::vector<uint8_t> m_bytecode;
    ShaderStage m_stage = ShaderStage::autoDetect;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_d3dVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_d3dPixelShader;

    // HLSL source for runtime D3DCompile. Stored instead of pre-compiled
    // DXBC because the HLSL must be compiled in the same process context
    // where the shader will be used — pre-compiled DXBC from RSTB
    // generation causes driver errors on AMD.
    std::string m_hlslSource;
    std::string m_hlslEntryPoint;

    // Guards `ensureD3DShaders` against concurrent first-call from two
    // pipelines that share this ShaderModule. Without it, both threads
    // race the early-out check and both run `D3DCompile` + `m_bytecode.assign`
    // + `CreateVertexShader/PixelShader` against the same module — at best
    // wasted compile work, at worst use-after-free of one thread's
    // `compiledBlob` while the other is still reading from it.
    std::once_flag m_d3dInitFlag;

    // Create actual D3D11 shader objects from HLSL source or stored DXBC.
    // Called from makePipeline. `outError`, if non-null, is populated with
    // the D3DCompile error log on compile failure so the caller can route
    // it to Context::m_lastError.
    void ensureD3DShaders(ID3D11Device* device, std::string* outError = nullptr)
    {
        std::call_once(m_d3dInitFlag,
                       [&]() { ensureD3DShadersImpl(device, outError); });
    }

    void ensureD3DShadersImpl(ID3D11Device* device, std::string* outError)
    {
        const void* code = m_bytecode.data();
        SIZE_T codeSize = m_bytecode.size();
        Microsoft::WRL::ComPtr<ID3DBlob> compiledBlob;

        if (!m_hlslSource.empty())
        {
            // Normalize line endings — SPIRV-Cross outputs \r\n on Windows.
            m_hlslSource.erase(
                std::remove(m_hlslSource.begin(), m_hlslSource.end(), '\r'),
                m_hlslSource.end());

            const char* target =
                (m_stage == ShaderStage::fragment) ? "ps_5_0" : "vs_5_0";
            const char* entry =
                m_hlslEntryPoint.empty() ? "main" : m_hlslEntryPoint.c_str();
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            HRESULT hr = D3DCompile(m_hlslSource.c_str(),
                                    m_hlslSource.size(),
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    entry,
                                    target,
                                    D3DCOMPILE_ENABLE_STRICTNESS |
                                        D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                    0,
                                    compiledBlob.GetAddressOf(),
                                    errors.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                code = compiledBlob->GetBufferPointer();
                codeSize = compiledBlob->GetBufferSize();
                // Store compiled bytecode for CreateInputLayout.
                m_bytecode.assign(static_cast<const uint8_t*>(code),
                                  static_cast<const uint8_t*>(code) + codeSize);
            }
            else
            {
                if (outError)
                {
                    const char* errMsg = errors
                                             ? static_cast<const char*>(
                                                   errors->GetBufferPointer())
                                             : "(no error log)";
                    char buf[1024];
                    snprintf(buf,
                             sizeof(buf),
                             "D3DCompile failed (entry=%s target=%s "
                             "hr=0x%08x): %s",
                             entry,
                             target,
                             static_cast<unsigned>(hr),
                             errMsg);
                    *outError = buf;
                }
                return;
            }
        }
        else if (m_bytecode.empty())
        {
            return;
        }

        if (m_stage == ShaderStage::fragment)
        {
            device->CreatePixelShader(code,
                                      codeSize,
                                      nullptr,
                                      m_d3dPixelShader.GetAddressOf());
        }
        else if (m_stage == ShaderStage::vertex)
        {
            device->CreateVertexShader(code,
                                       codeSize,
                                       nullptr,
                                       m_d3dVertexShader.GetAddressOf());
        }
        else
        {
            HRESULT hr =
                device->CreateVertexShader(code,
                                           codeSize,
                                           nullptr,
                                           m_d3dVertexShader.GetAddressOf());
            if (FAILED(hr))
            {
                device->CreatePixelShader(code,
                                          codeSize,
                                          nullptr,
                                          m_d3dPixelShader.GetAddressOf());
            }
        }
    }
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::ShaderModule m_wgpuShaderModule;
#endif
#if defined(ORE_BACKEND_VK)
    VkShaderModule m_vkShaderModule = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    PFN_vkDestroyShaderModule m_vkDestroyShaderModule = nullptr;
#endif
#if defined(ORE_BACKEND_D3D12)
    std::vector<uint8_t> m_d3dBytecode;
    bool m_d3dIsVertex = false; // True = VS, False = PS.
#endif

public:
    void onRefCntReachedZero() const;
};

} // namespace rive::ore
