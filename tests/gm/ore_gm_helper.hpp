/*
 * Copyright 2025 Rive
 */

// Shared helper for cross-platform Ore GM tests. Handles backend detection
// and Ore context creation so each GM only contains Ore API code + shaders.
// Supports multiple compiled backends in the same binary (e.g. Metal + GL on
// macOS) and picks the right one at runtime based on the active TestingWindow.

#pragma once

#include "common/testing_window.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <unordered_map>

// True when any Ore backend is compiled. Multiple backends may be active
// simultaneously (e.g. Metal + GL on macOS). Source of truth for every GM.
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK) ||                    \
    defined(ORE_BACKEND_RHI)
#define ORE_GM_HAS_BACKEND 1
#else
#define ORE_GM_HAS_BACKEND 0
#endif

#if ORE_GM_HAS_BACKEND
#include "rive/renderer/ore/ore_context.hpp"
#include <memory>
#endif
#if defined(ORE_BACKEND_METAL)
#include "rive/renderer/ore/ore_context_metal.hpp"
#endif
#if defined(ORE_BACKEND_GL)
#include "rive/renderer/ore/ore_context_gl.hpp"
#endif
#if defined(ORE_BACKEND_D3D11)
#include "rive/renderer/ore/ore_context_d3d11.hpp"
#endif
#if defined(ORE_BACKEND_D3D12)
#include "rive/renderer/ore/ore_context_d3d12.hpp"
#endif
#if defined(ORE_BACKEND_WGPU)
#include "rive/renderer/ore/ore_context_wgpu.hpp"
#endif
#if defined(ORE_BACKEND_VK)
#include "rive/renderer/ore/ore_context_vulkan.hpp"
#endif
#if defined(ORE_BACKEND_METAL)
#include "rive/renderer/metal/render_context_metal_impl.h"
#endif
#if defined(ORE_BACKEND_GL)
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#endif
#if defined(ORE_BACKEND_D3D11)
#include "rive/renderer/d3d11/render_context_d3d_impl.hpp"
#include <d3dcompiler.h>
#include <vector>
#endif
#if defined(ORE_BACKEND_D3D12)
#include "rive/renderer/d3d12/render_context_d3d12_impl.hpp"
#include <d3dcompiler.h>
#include <vector>
#endif
#if defined(ORE_BACKEND_WGPU)
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"
#endif
#if defined(ORE_BACKEND_VK)
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#endif

#if ORE_GM_HAS_BACKEND
#include "ore_gm_shaders.rstb.hpp"
#include "rive/renderer/ore/ore_rstb_entry_container.hpp"
#include "rive/assets/shader_asset.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#endif

namespace ore_gm
{

// Returns true if the active TestingWindow backend matches any compiled
// Ore backend.
inline bool isOreBackendActive()
{
#if defined(ORE_BACKEND_RHI)
    if (TestingWindow::isUnreal())
    {
        return true;
    }
#endif
    [[maybe_unused]] auto b = TestingWindow::backend();
#if defined(ORE_BACKEND_METAL)
    if (b == TestingWindow::Backend::metal)
    {
        return true;
    }
#endif
#if defined(ORE_BACKEND_GL)
    if (b == TestingWindow::Backend::gl || b == TestingWindow::Backend::angle)
    {
        return true;
    }
#endif
#if defined(ORE_BACKEND_D3D11)
    if (b == TestingWindow::Backend::d3d)
    {
        return true;
    }
#endif
#if defined(ORE_BACKEND_D3D12)
    if (b == TestingWindow::Backend::d3d12)
    {
        return true;
    }
#endif
#if defined(ORE_BACKEND_WGPU)
    if (b == TestingWindow::Backend::wgpu || b == TestingWindow::Backend::dawn)
    {
        return true;
    }
#endif
#if defined(ORE_BACKEND_VK)
    if (b == TestingWindow::Backend::vk ||
        b == TestingWindow::Backend::moltenvk ||
        b == TestingWindow::Backend::swiftshader)
    {
        return true;
    }
#endif
    return false;
}

// Holds the Ore context. At runtime, creates the appropriate backend context
// based on the active TestingWindow backend.
struct OreGMContext
{
    // Creates the Ore context from the active TestingWindow's render context.
    // Returns false if the backend doesn't match or context creation fails.
    bool ensureContext(rive::gpu::RenderContext* renderContext)
    {

#if ORE_GM_HAS_BACKEND
        if (!renderContext || !isOreBackendActive())
            return false;

#if defined(ORE_BACKEND_RHI)
        if (TestingWindow::isUnreal())
        {
            return true;
        }
#endif
        [[maybe_unused]] auto b = TestingWindow::backend();

#if defined(ORE_BACKEND_METAL)
        if (b == TestingWindow::Backend::metal)
        {
            auto* impl =
                renderContext
                    ->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
            auto queue = (__bridge id<MTLCommandQueue>)TestingWindow::Get()
                             ->metalQueue();
            assert(queue != nil);
            impl->setCommandQueue(queue);
            return true;
        }
#endif
#if defined(ORE_BACKEND_GL)
        if (b == TestingWindow::Backend::gl ||
            b == TestingWindow::Backend::angle)
        {
            return true;
        }
#endif
#if defined(ORE_BACKEND_D3D11)
        if (b == TestingWindow::Backend::d3d)
        {
            return true;
        }
#endif
#if defined(ORE_BACKEND_D3D12)
        if (b == TestingWindow::Backend::d3d12)
        {
            return true;
        }
#endif
#if defined(ORE_BACKEND_WGPU)
        if (b == TestingWindow::Backend::wgpu ||
            b == TestingWindow::Backend::dawn)
        {
            return true;
        }
#endif
#if defined(ORE_BACKEND_VK)
        if (b == TestingWindow::Backend::vk ||
            b == TestingWindow::Backend::moltenvk ||
            b == TestingWindow::Backend::swiftshader)
        {
            return true;
        }
#endif
        return false;
#else
        (void)renderContext;
        return false;
#endif
    }

    // Wrapper around ore::Context::beginFrame() that plugs Ore into the
    // host's command buffer/encoder when supported. On Vulkan, this enables
    // cross-engine read-after-write (e.g. Rive renders into a canvas then
    // Ore samples it) by keeping Rive and Ore in the same submission.
    // Falls back to owned-CB mode when no external CB is available.
    void beginFrame(rive::gpu::RenderContext* renderContext)
    {
        TestingWindow::Get()->beginOreFrame();
    }

    void endFrame(rive::gpu::RenderContext* renderContext)
    {
        TestingWindow::Get()->endOreFrame();
    }
};

// Ore leaves GL state behind that Rive's own cache does not track, so the
// next flush would skip updating it and render black. The backend owns the
// cleanup; every other backend no-ops.
inline void invalidateGLStateAfterOre(rive::gpu::RenderContext* renderContext)
{
    renderContext->impl()->scrubStateAfterOre();
}

#if defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_D3D12)
// Compile HLSL source to DXBC bytecode at runtime using D3DCompile.
// target is e.g. "vs_5_0" or "ps_5_0".
inline std::vector<uint8_t> compileHLSL(const char* source,
                                        const char* entry,
                                        const char* target)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr = D3DCompile(source,
                            strlen(source),
                            nullptr,
                            nullptr,
                            nullptr,
                            entry,
                            target,
                            flags,
                            0,
                            blob.GetAddressOf(),
                            errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors)
            fprintf(stderr,
                    "HLSL compile error (%s %s): %s\n",
                    entry,
                    target,
                    static_cast<const char*>(errors->GetBufferPointer()));
        assert(false && "HLSL compilation failed");
        return {};
    }
    const uint8_t* data = static_cast<const uint8_t*>(blob->GetBufferPointer());
    return std::vector<uint8_t>(data, data + blob->GetBufferSize());
}
#endif

// ── RSTB shader loader ───────────────────────────────────────────────────────
//
// Loads pre-compiled shader variants from the embedded RSTB header generated
// by the ore_gm_rstb_test in scripting_workspace.
//
// ShaderTarget constants (must match RSTB format):
//   0=WGSL, 1=GLSL_ES3, 2=MSL, 3=HLSL_SM5, 5=SPIR-V

#if ORE_GM_HAS_BACKEND

// Keeps GM shader asset ids clear of riv asset ids and 0 (unset).
constexpr uint32_t kOreGMShaderAssetIdBase = 0x80000000u;

enum OreGMShader : uint32_t
{
    kTriangle = 0,
    kDepth = 1,
    kImageView = 2,
    kCubemap = 3,
    kMrt = 4,
    kMrtBlit = 5,
    kBindingWitness = 6,
    kMixedKindWitness = 7,
    kMultiGroupWitness = 8,
    kShadowSamplerWitness = 9,
    kVSTextureWitness = 10,
    kDynamicUBOWitness = 11,
    kArray2DWitness = 12,
};

struct OreGMShaderResult
{
    rive::rcp<rive::ore::ShaderModule> vsModule;
    rive::rcp<rive::ore::ShaderModule> psModule;
    const char* vsEntryPoint = "vs_main";
    const char* fsEntryPoint = "fs_main";
};

// Lazy per-shader ShaderAsset cache. The fixture header
// `ore_gm_shaders.rstb.hpp` embeds N single-shader RSTB v4 blobs concatenated
// with an offset table indexed by `OreGMShader`.
inline rive::ShaderAsset& getRstbAssetForShader(uint32_t shaderId)
{
    static std::array<rive::ShaderAsset, ore_gm_shaders::kShaderCount> assets;
    static std::array<bool, ore_gm_shaders::kShaderCount> parsed{};
    assert(shaderId < ore_gm_shaders::kShaderCount);
    if (!parsed[shaderId])
    {
        parsed[shaderId] = true;
        // ShaderAsset::decode expects a SignedContentHeader envelope. Prepend
        // an unsigned envelope (flags=0) around the per-shader RSTB slice.
        uint32_t off = ore_gm_shaders::kShaderOffsets[shaderId];
        uint32_t size = ore_gm_shaders::kShaderOffsets[shaderId + 1] - off;
        rive::SimpleArray<uint8_t> data(size + 1);
        data[0] = 0x00;
        memcpy(data.data() + 1, ore_gm_shaders::kShaderData + off, size);
        bool ok = assets[shaderId].decode(data, nullptr);
        assert(ok && "ore_gm fixture decode failed");
        (void)ok;
    }
    return assets[shaderId];
}

/// Map TestingWindow backend to RSTB ShaderTarget.
inline uint8_t shaderTargetForBackend()
{
#if defined(ORE_BACKEND_RHI)
    if (TestingWindow::isUnreal())
        return 3; // HLSL SM5
#endif
    [[maybe_unused]] auto b = TestingWindow::backend();
#if defined(ORE_BACKEND_METAL)
    if (b == TestingWindow::Backend::metal)
        return 2; // MSL
#endif
#if defined(ORE_BACKEND_GL)
    if (b == TestingWindow::Backend::gl || b == TestingWindow::Backend::angle)
        return 1; // GLSL ES3
#endif
#if defined(ORE_BACKEND_D3D11)
    if (b == TestingWindow::Backend::d3d)
        return 3; // HLSL SM5
#endif
#if defined(ORE_BACKEND_D3D12)
    if (b == TestingWindow::Backend::d3d12)
        return 3; // HLSL SM5
#endif
#if defined(ORE_BACKEND_WGPU)
    if (b == TestingWindow::Backend::wgpu || b == TestingWindow::Backend::dawn)
        return 0; // WGSL
#endif
#if defined(ORE_BACKEND_VK)
    if (b == TestingWindow::Backend::vk ||
        b == TestingWindow::Backend::moltenvk ||
        b == TestingWindow::Backend::swiftshader)
        return 5; // SPIR-V
#endif
    return 0xFF; // Unknown.
}

/// WGSL entry point names per shader ID (for MSL, WGSL, SPIR-V targets).
inline std::pair<const char*, const char*> wgslEntryPoints(uint32_t shaderId)
{
    switch (shaderId)
    {
        case kCubemap:
            return {"cube_vs", "cube_fs"};
        case kMrtBlit:
            return {"blit_vs", "blit_fs"};
        default:
            return {"vs_main", "fs_main"};
    }
}

/// RSTB ShaderTarget IDs. The full table for both the shader-source
/// variants and the per-target binding-map sidecars is scattered across
/// `ore_gm_rstb_test.cpp` (writer side) and `shaderTargetForBackend` /
/// `bindingMapTargetFor` (reader side); kept here so a single grep finds
/// every magic number.
///
///   Source variants (the compiled shader bytes):
///     0  WGSL              (passthrough WGSL for the WGPU backend)
///     1  GLSL ES3          (compiled GLSL for the GL backend)
///     2  MSL               (compiled MSL for the Metal backend)
///     3  HLSL SM5          (SPIRV-Cross output for D3D11 / D3D12)
///     4  unused — reserved for future HLSL SM6
///     5  SPIR-V            (compiled SPIR-V for the Vulkan backend)
///     6-9 reserved — future source variants
///
///   Binding-map sidecars (paired 1:1 with a source target;
///   `findShader(sidecarTarget)` returns the `ore::BindingMap` blob
///   the runtime parses with `BindingMap::fromBlob`):
///     10 MSL binding map        (paired with source 2)
///     11 GLSL binding map       (paired with source 1)
///     12 HLSL binding map       (paired with source 3)
///     13 SPIR-V binding map     (paired with source 5)
///     14 GLSL VS link-fixup     (program-link `glUniform*` table)
///     15 GLSL FS link-fixup     (paired with 14)
///     16 WGSL identity map      (paired with source 0 — reflection only)
///
///   255 = "no sidecar for this source" (currently unused —
///   `bindingMapTargetFor` once returned this for non-allocator targets;
///   today every active source target has a paired sidecar).
inline uint8_t bindingMapTargetFor(uint8_t sourceTarget)
{
    switch (sourceTarget)
    {
        case 0:
            return 16; // WGSL → WGSL identity binding map
        case 1:
            return 11; // GLSL → GLSL binding map
        case 2:
            return 10; // MSL → MSL binding map
        case 3:
            return 12; // HLSL → HLSL binding map
        case 5:
            return 13; // SPIR-V → SPIR-V binding map
        default:
            return 255;
    }
}

/// Load a compiled shader from the embedded RSTB for the active backend.
/// Parses the RSTB v4 entry-point container (ore_rstb_entry_container.hpp):
/// whole-module targets (WGSL/MSL/SPIR-V) build one module shared by both
/// stages; per-entry targets (GLSL/HLSL) build a module per entry. GMs are
/// single-entry-per-stage and use non-keyword names, so the WebGPU entry-point
/// names come from `wgslEntryPoints` (logical == physical here).
inline OreGMShaderResult loadShader(rive::ore::Context& ctx, uint32_t shaderId)
{
    using namespace rive::ore;
    OreGMShaderResult result{};

    auto& asset = getRstbAssetForShader(shaderId);
    uint8_t target = shaderTargetForBackend();

    auto blob = asset.findShader(target);
    if (blob.empty())
        return result;
    const uint8_t* blobData = blob.data();
    uint32_t blobSize = static_cast<uint32_t>(blob.size());

    // Binding-map sidecar (mandatory).
    uint8_t bmTarget = bindingMapTargetFor(target);
    auto bindingMapBlob = (bmTarget == 255) ? rive::Span<const uint8_t>{}
                                            : asset.findShader(bmTarget);
    assert(bmTarget == 255 || !bindingMapBlob.empty());
    const uint8_t* bindingMapBytes =
        bindingMapBlob.empty() ? nullptr : bindingMapBlob.data();
    uint32_t bindingMapSize = static_cast<uint32_t>(bindingMapBlob.size());

    // GL program-link fixup tables (only present for GLSL source target).
    auto vsGLFixupBlob =
        (target == 1) ? asset.findShader(14) : rive::Span<const uint8_t>{};
    auto fsGLFixupBlob =
        (target == 1) ? asset.findShader(15) : rive::Span<const uint8_t>{};

    auto propagatePairs = [&](rive::ore::ShaderModule* mod) {
        if (!mod)
            return;
        auto pairs = asset.textureSamplerPairs();
        for (size_t i = 0; i < pairs.size(); i++)
        {
            mod->m_textureSamplerPairs.push_back({pairs[i].texGroup,
                                                  pairs[i].texBinding,
                                                  pairs[i].sampGroup,
                                                  pairs[i].sampBinding});
        }
    };

    auto names = wgslEntryPoints(shaderId);

    // Per-entry targets: GLSL (1), HLSL (3). Build one module per entry; GMs
    // use the first vertex + first fragment entry.
    if (target == 1 || target == 3)
    {
        std::vector<RstbEntryView> views;
        if (!parsePerEntryContainer(blobData, blobSize, views))
            return result;
        for (const auto& v : views)
        {
            const bool isVtx = (v.stage == 0);
            if (isVtx && result.vsModule)
                continue;
            if (!isVtx && result.psModule)
                continue;
            ShaderModuleDesc desc{};
            desc.stage = isVtx ? ShaderStage::vertex : ShaderStage::fragment;
            desc.shaderAssetId = kOreGMShaderAssetIdBase + shaderId;
            desc.bindingMapBytes = bindingMapBytes;
            desc.bindingMapSize = bindingMapSize;
            if (target == 3)
            {
                desc.hlslSource = reinterpret_cast<const char*>(v.source);
                desc.hlslSourceSize = v.sourceSize;
                desc.hlslEntryPoint = v.physical.c_str();
            }
            else
            {
                desc.code = v.source;
                desc.codeSize = v.sourceSize;
                auto fx = isVtx ? vsGLFixupBlob : fsGLFixupBlob;
                desc.glFixupBytes = fx.empty() ? nullptr : fx.data();
                desc.glFixupSize = static_cast<uint32_t>(fx.size());
            }
            auto mod = ctx.makeShaderModule(desc);
            propagatePairs(mod.get());
            if (isVtx)
                result.vsModule = mod;
            else
                result.psModule = mod;
        }
        // GL compiles `main`; D3D bakes the entry into the module and ignores
        // PipelineDesc's entry name, so a representative name is fine here.
        result.vsEntryPoint = (target == 1) ? "main" : names.first;
        result.fsEntryPoint = (target == 1) ? "main" : names.second;
        return result;
    }

    // Whole-module targets: MSL (2), WGSL (0), SPIR-V (5). One module, both
    // stages, selected by name at the driver.
    std::vector<RstbEntryView> views;
    const uint8_t* src = nullptr;
    uint32_t srcLen = 0;
    if (!parseWholeModuleContainer(blobData, blobSize, views, &src, &srcLen))
        return result;
    ShaderModuleDesc desc{};
    desc.shaderAssetId = kOreGMShaderAssetIdBase + shaderId;
    desc.code = src;
    desc.codeSize = srcLen;
    if (target == 0)
        desc.language = ShaderLanguage::wgsl;
    desc.bindingMapBytes = bindingMapBytes;
    desc.bindingMapSize = bindingMapSize;
    result.vsModule = ctx.makeShaderModule(desc);
    result.psModule = result.vsModule; // Same module for VS + PS.
    result.vsEntryPoint = names.first;
    result.fsEntryPoint = names.second;
    propagatePairs(result.vsModule.get());
    return result;
}

// Build a `BindGroupLayout` from a shader's `BindingMap` for a given group.
//
// `dynamicUBOBindings` (optional): array of WGSL @binding values within
// `group` whose UBO entries should set `hasDynamicOffset = true`. Mirrors
// the legacy `PipelineDesc::dynamicUBOs` declaration but scoped per-layout.
inline rive::rcp<rive::ore::BindGroupLayout> makeLayoutFromShader(
    rive::ore::Context& ctx,
    rive::ore::ShaderModule* shader,
    uint32_t group,
    const uint32_t* dynamicUBOBindings = nullptr,
    uint32_t dynamicUBOCount = 0)
{
    return rive::ore::makeBindGroupLayoutFromShader(ctx,
                                                    shader,
                                                    group,
                                                    dynamicUBOBindings,
                                                    dynamicUBOCount);
}

// Shared triangle pass used by the deferred GMs.
struct TriVertex
{
    float x, y;
    float r, g, b, a;
};

inline constexpr TriVertex kTriVertices[] = {
    {0.0f, 0.6f, 1.0f, 0.2f, 0.2f, 1.0f},
    {-0.6f, -0.6f, 0.2f, 1.0f, 0.2f, 1.0f},
    {0.6f, -0.6f, 0.2f, 0.2f, 1.0f, 1.0f},
};

// desc points into attrs and layout, so this object must stay alive through
// makePipeline and cannot be copied.
struct TrianglePipeline
{
    TrianglePipeline(const OreGMShaderResult& shader,
                     rive::ore::TextureFormat targetFormat,
                     const char* label)
    {
        layout.stride = sizeof(TriVertex);
        layout.stepMode = rive::ore::VertexStepMode::vertex;
        layout.attributes = attrs;
        layout.attributeCount = 2;
        desc.vertexModule = shader.vsModule.get();
        desc.fragmentModule = shader.psModule.get();
        desc.vertexEntryPoint = shader.vsEntryPoint;
        desc.fragmentEntryPoint = shader.fsEntryPoint;
        desc.vertexBuffers = &layout;
        desc.vertexBufferCount = 1;
        desc.topology = rive::ore::PrimitiveTopology::triangleList;
        desc.colorTargets[0].format = targetFormat;
        desc.colorCount = 1;
        desc.label = label;
    }
    TrianglePipeline(const TrianglePipeline&) = delete;
    TrianglePipeline& operator=(const TrianglePipeline&) = delete;

    rive::ore::VertexAttribute attrs[2] = {
        {offsetof(TriVertex, x), 0, rive::ore::VertexFormat::float2},
        {offsetof(TriVertex, r), 1, rive::ore::VertexFormat::float4},
    };
    rive::ore::VertexBufferLayout layout{};
    rive::ore::PipelineDesc desc{};
};

#endif // ORE_BACKEND_*

} // namespace ore_gm
