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
#include <cstdio>
#include <cstring>
#include <unordered_map>

// Include Ore headers when any backend is compiled.
// Multiple backends may be active simultaneously (e.g. Metal + GL on macOS).
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
#include "rive/renderer/ore/ore_context.hpp"
#include <optional>
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

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
#include "ore_gm_shaders.rstb.hpp"
#include "rive/assets/shader_asset.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#endif

namespace ore_gm
{

// Returns true if the active TestingWindow backend matches any compiled
// Ore backend.
inline bool isOreBackendActive()
{
    auto b = TestingWindow::backend();
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
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
    std::optional<rive::ore::Context> oreContext;
#endif

    // Creates the Ore context from the active TestingWindow's render context.
    // Returns false if the backend doesn't match or context creation fails.
    bool ensureContext(rive::gpu::RenderContext* renderContext)
    {
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
        if (oreContext.has_value())
            return true;

        if (!renderContext || !isOreBackendActive())
            return false;

        auto b = TestingWindow::backend();

#if defined(ORE_BACKEND_METAL)
        if (b == TestingWindow::Backend::metal)
        {
            auto* impl =
                renderContext
                    ->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
            auto queue = (__bridge id<MTLCommandQueue>)TestingWindow::Get()
                             ->metalQueue();
            assert(queue != nil);
            oreContext.emplace(
                rive::ore::Context::createMetal(impl->gpu(), queue));
            return true;
        }
#endif
#if defined(ORE_BACKEND_GL)
        if (b == TestingWindow::Backend::gl ||
            b == TestingWindow::Backend::angle)
        {
            oreContext.emplace(rive::ore::Context::createGL());
            return true;
        }
#endif
#if defined(ORE_BACKEND_D3D11)
        if (b == TestingWindow::Backend::d3d)
        {
            auto* impl =
                renderContext
                    ->static_impl_cast<rive::gpu::RenderContextD3DImpl>();
            oreContext.emplace(
                rive::ore::Context::createD3D11(impl->gpu(),
                                                impl->gpuContext()));
            return true;
        }
#endif
#if defined(ORE_BACKEND_D3D12)
        if (b == TestingWindow::Backend::d3d12)
        {
            auto* impl =
                renderContext
                    ->static_impl_cast<rive::gpu::RenderContextD3D12Impl>();
            oreContext.emplace(
                rive::ore::Context::createD3D12(impl->device().Get(),
                                                impl->commandQueue()));
            return true;
        }
#endif
#if defined(ORE_BACKEND_WGPU)
        if (b == TestingWindow::Backend::wgpu ||
            b == TestingWindow::Backend::dawn)
        {
            auto* impl =
                renderContext
                    ->static_impl_cast<rive::gpu::RenderContextWebGPUImpl>();
            oreContext.emplace(rive::ore::Context::createWGPU(
                impl->device(),
                impl->queue(),
                impl->capabilities().backendType));
            return true;
        }
#endif
#if defined(ORE_BACKEND_VK)
        if (b == TestingWindow::Backend::vk ||
            b == TestingWindow::Backend::moltenvk ||
            b == TestingWindow::Backend::swiftshader)
        {
            auto* impl =
                renderContext
                    ->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
            rive::gpu::VulkanContext* vkCtx = impl->vulkanContext();
            auto* win = TestingWindow::Get();
            VkQueue queue = static_cast<VkQueue>(win->vulkanGraphicsQueue());
            uint32_t queueFamily = win->vulkanGraphicsQueueFamilyIndex();
            auto pfnGetInstanceProcAddr =
                reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    win->vulkanGetInstanceProcAddr());
            if (queue == VK_NULL_HANDLE || pfnGetInstanceProcAddr == nullptr)
                return false;
            oreContext.emplace(
                rive::ore::Context::createVulkan(vkCtx->instance,
                                                 vkCtx->physicalDevice,
                                                 vkCtx->device,
                                                 queue,
                                                 queueFamily,
                                                 vkCtx->allocator(),
                                                 pfnGetInstanceProcAddr));
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
    void beginFrame()
    {
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
        assert(oreContext.has_value());
#if defined(ORE_BACKEND_VK)
        auto b = TestingWindow::backend();
        if (b == TestingWindow::Backend::vk ||
            b == TestingWindow::Backend::moltenvk ||
            b == TestingWindow::Backend::swiftshader)
        {
            VkCommandBuffer cb = static_cast<VkCommandBuffer>(
                TestingWindow::Get()->vulkanCurrentCommandBuffer());
            if (cb != VK_NULL_HANDLE)
            {
                oreContext->beginFrame(cb);
                return;
            }
        }
#endif
#if defined(ORE_BACKEND_D3D12)
        if (TestingWindow::backend() == TestingWindow::Backend::d3d12)
        {
            auto* cl = static_cast<ID3D12GraphicsCommandList*>(
                TestingWindow::Get()->d3d12CurrentCommandList());
            if (cl != nullptr)
            {
                oreContext->beginFrame(cl);
                return;
            }
        }
#endif
#if defined(ORE_BACKEND_WGPU)
        {
            auto b = TestingWindow::backend();
            if (b == TestingWindow::Backend::wgpu ||
                b == TestingWindow::Backend::dawn)
            {
                auto rawEncoder = static_cast<WGPUCommandEncoder>(
                    TestingWindow::Get()->wgpuCurrentCommandEncoder());
                if (rawEncoder != nullptr)
                {
                    // AddRef the host's encoder so we own a ref to hand to
                    // Ore; Acquire() takes ownership of that ref without
                    // incrementing again. Matches RenderContextWebGPUImpl's
                    // externalCommandBuffer adoption pattern.
#if (defined(RIVE_WEBGPU) && RIVE_WEBGPU > 1) || defined(RIVE_DAWN)
                    wgpuCommandEncoderAddRef(rawEncoder);
#else
                    wgpuCommandEncoderReference(rawEncoder);
#endif
                    oreContext->beginFrame(
                        wgpu::CommandEncoder::Acquire(rawEncoder));
                    return;
                }
            }
        }
#endif
        oreContext->beginFrame();
#endif
    }

    void endFrame()
    {
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
        assert(oreContext.has_value());
        oreContext->endFrame();
#endif
    }
};

// Ore's GL backend modifies GL state (blend, depth, stencil, cull, front face
// winding, etc.) that Rive's GLState cache tracks. After Ore rendering,
// invalidate the cache so Rive re-issues all GL state changes on the next
// flush. Without this, the MSAA path (which relies on correct cached state for
// culling and stencil) renders black because GLState skips state updates it
// thinks are redundant.
// For WGPU, command submission is handled by Context::endFrame() and there
// is no shared GL state cache to invalidate.
inline void invalidateGLStateAfterOre(
    [[maybe_unused]] rive::gpu::RenderContext* renderContext)
{
#if defined(ORE_BACKEND_GL)
    auto b = TestingWindow::backend();
    if (b == TestingWindow::Backend::gl || b == TestingWindow::Backend::angle)
    {
        // Ensure all Ore GPU commands are complete before returning to Rive.
        // On some MSAA drivers, pending Ore FBO operations can interfere with
        // subsequent Rive MSAA flush.
        glFinish();

        // Unbind sampler objects from all texture units. Sampler objects are
        // global state (not per-FBO/VAO) and not restored by
        // Context::endFrame() or tracked by GLState. A stale sampler can
        // override Rive's texture sampling parameters and cause black renders
        // on the MSAA path.
        for (int i = 0; i < 16; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glBindSampler(i, 0);
        }
        glActiveTexture(GL_TEXTURE0);

        renderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>()
            ->invalidateGLState();
    }
#endif
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

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)

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

// Reuse ShaderAsset for RSTB parsing — single source of truth for
// the binary format (handles v1 and v2, including tagged metadata sections).
inline rive::ShaderAsset& getRstbAsset()
{
    static rive::ShaderAsset asset;
    static bool parsed = false;
    if (!parsed)
    {
        parsed = true;
        // ShaderAsset::decode expects a SignedContentHeader envelope
        // (`[flags:1][sig:64?][RSTB...]`). ore_gm_rstb_test emits raw RSTB,
        // so prepend an unsigned envelope (flags=0, no signature).
        rive::SimpleArray<uint8_t> data(ore_gm_shaders::kDataLen + 1);
        data[0] = 0x00;
        memcpy(data.data() + 1,
               ore_gm_shaders::kData,
               ore_gm_shaders::kDataLen);
        asset.decode(data, nullptr);
    }
    return asset;
}

/// Map TestingWindow backend to RSTB ShaderTarget.
inline uint8_t shaderTargetForBackend()
{
    auto b = TestingWindow::backend();
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

/// RSTB ShaderTarget IDs (RFC §8.1). The full table for both the
/// shader-source variants and the per-target binding-map sidecars is
/// scattered across `ore_gm_rstb_test.cpp` (writer side) and
/// `shaderTargetForBackend` / `bindingMapTargetFor` (reader side); kept
/// here so a single grep finds every magic number.
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
///   `findShader(id, sidecarTarget)` returns the `ore::BindingMap` blob
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
/// Returns modules and entry point names ready for use in a PipelineDesc.
inline OreGMShaderResult loadShader(rive::ore::Context& ctx, uint32_t shaderId)
{
    using namespace rive::ore;
    OreGMShaderResult result{};

    auto& asset = getRstbAsset();
    uint8_t target = shaderTargetForBackend();

    auto blob = asset.findShader(shaderId, target);
    if (blob.empty())
        return result;

    const char* blobData = reinterpret_cast<const char*>(blob.data());
    uint32_t blobSize = static_cast<uint32_t>(blob.size());

    // Look up the binding-map sidecar — required by `makeShaderModule`
    // (RFC §14.4). `bindingMapTargetFor` returns 255 only for source
    // targets we don't ship; for any active runtime target there must be
    // a paired sidecar in the RSTB.
    uint8_t bmTarget = bindingMapTargetFor(target);
    auto bindingMapBlob = (bmTarget == 255)
                              ? rive::Span<const uint8_t>{}
                              : asset.findShader(shaderId, bmTarget);
    assert(bmTarget == 255 || !bindingMapBlob.empty());
    const uint8_t* bindingMapBytes =
        bindingMapBlob.empty() ? nullptr : bindingMapBlob.data();
    uint32_t bindingMapSize = static_cast<uint32_t>(bindingMapBlob.size());

    // GL program-link fixup tables (sidecar targets 14 = VS, 15 = FS).
    // Only the GLSL source target carries them; lookup is a cheap no-op
    // for other backends.
    auto vsGLFixupBlob = (target == 1) ? asset.findShader(shaderId, 14)
                                       : rive::Span<const uint8_t>{};
    auto fsGLFixupBlob = (target == 1) ? asset.findShader(shaderId, 15)
                                       : rive::Span<const uint8_t>{};
    const uint8_t* vsGLFixupBytes =
        vsGLFixupBlob.empty() ? nullptr : vsGLFixupBlob.data();
    uint32_t vsGLFixupSize = static_cast<uint32_t>(vsGLFixupBlob.size());
    const uint8_t* fsGLFixupBytes =
        fsGLFixupBlob.empty() ? nullptr : fsGLFixupBlob.data();
    uint32_t fsGLFixupSize = static_cast<uint32_t>(fsGLFixupBlob.size());

    // Helper: propagate texture-sampler pairs from the RSTB asset into
    // a ShaderModule so the GL backend can map sampler slots correctly.
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

    // ── HLSL SM5 (target 3) ──
    // Blob: "vsEntry\0fsEntry\0vsHLSL\0fsHLSL"
    if (target == 3)
    {
        const char* vsEntry = blobData;
        const char* vsEnd =
            static_cast<const char*>(memchr(vsEntry, '\0', blobSize));
        if (!vsEnd)
            return result;

        const char* fsEntry = vsEnd + 1;
        uint32_t remaining =
            blobSize - static_cast<uint32_t>(fsEntry - blobData);
        const char* fsEnd =
            static_cast<const char*>(memchr(fsEntry, '\0', remaining));
        if (!fsEnd)
            return result;

        const char* vsHLSL = fsEnd + 1;
        remaining = blobSize - static_cast<uint32_t>(vsHLSL - blobData);
        const char* vsHLSLEnd =
            static_cast<const char*>(memchr(vsHLSL, '\0', remaining));
        if (!vsHLSLEnd)
            return result;

        const char* fsHLSL = vsHLSLEnd + 1;
        uint32_t fsHLSLSize =
            blobSize - static_cast<uint32_t>(fsHLSL - blobData);

        ShaderModuleDesc vtxDesc{};
        vtxDesc.stage = ShaderStage::vertex;
        vtxDesc.hlslSource = vsHLSL;
        vtxDesc.hlslSourceSize = static_cast<uint32_t>(vsHLSLEnd - vsHLSL);
        vtxDesc.hlslEntryPoint = vsEntry;
        vtxDesc.bindingMapBytes = bindingMapBytes;
        vtxDesc.bindingMapSize = bindingMapSize;
        result.vsModule = ctx.makeShaderModule(vtxDesc);

        ShaderModuleDesc fragDesc{};
        fragDesc.stage = ShaderStage::fragment;
        fragDesc.hlslSource = fsHLSL;
        fragDesc.hlslSourceSize = fsHLSLSize;
        fragDesc.hlslEntryPoint = fsEntry;
        fragDesc.bindingMapBytes = bindingMapBytes;
        fragDesc.bindingMapSize = bindingMapSize;
        result.psModule = ctx.makeShaderModule(fragDesc);

        // HLSL entry points are from the blob.
        result.vsEntryPoint = vsEntry;
        result.fsEntryPoint = fsEntry;
        propagatePairs(result.vsModule.get());
        propagatePairs(result.psModule.get());
        return result;
    }

    // ── GLSL ES3 (target 1) ──
    // Blob: "vertexGLSL\0fragmentGLSL"
    if (target == 1)
    {
        const char* sep =
            static_cast<const char*>(memchr(blobData, '\0', blobSize - 1));
        if (!sep)
            return result;

        uint32_t vtxSize = static_cast<uint32_t>(sep - blobData);
        const char* fragData = sep + 1;
        uint32_t fragSize = blobSize - vtxSize - 1;

        ShaderModuleDesc vtxDesc{};
        vtxDesc.code = blobData;
        vtxDesc.codeSize = vtxSize;
        vtxDesc.stage = ShaderStage::vertex;
        vtxDesc.bindingMapBytes = bindingMapBytes;
        vtxDesc.bindingMapSize = bindingMapSize;
        vtxDesc.glFixupBytes = vsGLFixupBytes;
        vtxDesc.glFixupSize = vsGLFixupSize;
        result.vsModule = ctx.makeShaderModule(vtxDesc);

        ShaderModuleDesc fragDesc{};
        fragDesc.code = fragData;
        fragDesc.codeSize = fragSize;
        fragDesc.stage = ShaderStage::fragment;
        fragDesc.bindingMapBytes = bindingMapBytes;
        fragDesc.bindingMapSize = bindingMapSize;
        fragDesc.glFixupBytes = fsGLFixupBytes;
        fragDesc.glFixupSize = fsGLFixupSize;
        result.psModule = ctx.makeShaderModule(fragDesc);

        // GLSL always uses "main".
        result.vsEntryPoint = "main";
        result.fsEntryPoint = "main";
        propagatePairs(result.vsModule.get());
        propagatePairs(result.psModule.get());
        return result;
    }

    // ── Single-module targets: MSL (2), WGSL (0), SPIR-V (5) ──
    {
        ShaderModuleDesc desc{};
        desc.code = blobData;
        desc.codeSize = blobSize;
        if (target == 0)
            desc.language = ShaderLanguage::wgsl;
        desc.bindingMapBytes = bindingMapBytes;
        desc.bindingMapSize = bindingMapSize;
        result.vsModule = ctx.makeShaderModule(desc);
        result.psModule = result.vsModule; // Same module for VS + PS.

        auto [vs, fs] = wgslEntryPoints(shaderId);
        result.vsEntryPoint = vs;
        result.fsEntryPoint = fs;
        propagatePairs(result.vsModule.get());
        return result;
    }
}

// Map ResourceKind (binding-map enum) to BindingKind (public layout enum).
inline rive::ore::BindingKind bindingKindFromResource(rive::ore::ResourceKind k)
{
    using K = rive::ore::BindingKind;
    using R = rive::ore::ResourceKind;
    switch (k)
    {
        case R::UniformBuffer:
            return K::uniformBuffer;
        case R::StorageBufferRO:
            return K::storageBufferRO;
        case R::StorageBufferRW:
            return K::storageBufferRW;
        case R::SampledTexture:
            return K::sampledTexture;
        case R::StorageTexture:
            return K::storageTexture;
        case R::Sampler:
            return K::sampler;
        case R::ComparisonSampler:
            return K::comparisonSampler;
    }
    return K::uniformBuffer;
}

// Build a `BindGroupLayout` from a shader's `BindingMap` for a given group.
// Walks every entry whose `group == g`, copies kind / visibility / native
// slots into a `BindGroupLayoutEntry`, and calls `ctx.makeBindGroupLayout`.
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
    using namespace rive::ore;
    static constexpr int kMaxEntries = 16;
    BindGroupLayoutEntry entries[kMaxEntries]{};
    uint32_t n = 0;

    auto isDynamic = [&](uint32_t binding) -> bool {
        for (uint32_t i = 0; i < dynamicUBOCount; ++i)
            if (dynamicUBOBindings[i] == binding)
                return true;
        return false;
    };

    auto viewDimFromBindingMap =
        [](rive::ore::TextureViewDim d) -> rive::ore::TextureViewDimension {
        using D = rive::ore::TextureViewDim;
        using O = rive::ore::TextureViewDimension;
        switch (d)
        {
            case D::Cube:
                return O::cube;
            case D::CubeArray:
                return O::cubeArray;
            case D::D3:
                return O::texture3D;
            case D::D2Array:
                return O::array2D;
            case D::D1:
            case D::D2:
            case D::Undefined:
                return O::texture2D;
        }
        return O::texture2D;
    };

    auto sampleTypeFromBindingMap = [](rive::ore::TextureSampleType s)
        -> rive::ore::BindGroupLayoutEntry::SampleType {
        using S = rive::ore::TextureSampleType;
        using O = rive::ore::BindGroupLayoutEntry::SampleType;
        switch (s)
        {
            case S::UnfilterableFloat:
                return O::floatUnfilterable;
            case S::Depth:
                return O::depth;
            case S::Sint:
                return O::sint;
            case S::Uint:
                return O::uint;
            case S::Float:
            case S::Undefined:
                return O::floatFilterable;
        }
        return O::floatFilterable;
    };

    const BindingMap& bm = shader->m_bindingMap;
    for (size_t i = 0; i < bm.size() && n < kMaxEntries; ++i)
    {
        const BindingMap::Entry& e = bm.at(i);
        if (e.group != group)
            continue;
        BindGroupLayoutEntry& out = entries[n++];
        out.binding = e.binding;
        out.kind = bindingKindFromResource(e.kind);
        // Mirror the shader's declared visibility — narrower than this
        // would be rejected by validateLayoutsAgainstBindingMap.
        uint8_t vis = 0;
        if (e.stageMask & BindingMap::kStageVertex)
            vis |= StageVisibility::kVertex;
        if (e.stageMask & BindingMap::kStageFragment)
            vis |= StageVisibility::kFragment;
        if (e.stageMask & BindingMap::kStageCompute)
            vis |= StageVisibility::kCompute;
        out.visibility.mask = vis;
        out.hasDynamicOffset =
            (out.kind == BindingKind::uniformBuffer && isDynamic(e.binding));
        // Texture reflection — required for validation to accept cube /
        // 3D / array textures that don't match the texture2D default.
        out.textureViewDim = viewDimFromBindingMap(e.textureViewDim);
        out.textureSampleType = sampleTypeFromBindingMap(e.textureSampleType);
        out.textureMultisampled = e.textureMultisampled;
        // Pre-resolve native slots from the shader's binding map.
        const uint16_t vs =
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::VS)];
        const uint16_t fs =
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)];
        out.nativeSlotVS = (vs == BindingMap::kAbsent)
                               ? BindGroupLayoutEntry::kNativeSlotAbsent
                               : static_cast<uint32_t>(vs);
        out.nativeSlotFS = (fs == BindingMap::kAbsent)
                               ? BindGroupLayoutEntry::kNativeSlotAbsent
                               : static_cast<uint32_t>(fs);
    }

    BindGroupLayoutDesc desc;
    desc.groupIndex = group;
    desc.entries = entries;
    desc.entryCount = n;
    return ctx.makeBindGroupLayout(desc);
}

#endif // ORE_BACKEND_*

} // namespace ore_gm
