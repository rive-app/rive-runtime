/*
 * Copyright 2023 Rive
 */

#pragma once

#ifdef RIVE_WEBGPU

#include "rive/pls/webgpu/pls_render_context_webgpu_impl.hpp"

namespace rive::pls
{
// WebGPU implementation that uses Vulkan input attachments,
// VK_EXT_rasterization_order_attachment_access, and subpassLoad() for pixel local storage. These
// Vulkan features are accessed via nonstandard WebGPU APIs.
class PLSRenderContextWebGPUVulkan : public PLSRenderContextWebGPUImpl
{
public:
    rcp<PLSRenderTargetWebGPU> makeRenderTarget(wgpu::TextureFormat,
                                                uint32_t width,
                                                uint32_t height) override;

protected:
    wgpu::BindGroupLayout initPLSTextureBindGroup() override;

    wgpu::RenderPipeline makePLSDrawPipeline(rive::pls::DrawType drawType,
                                             wgpu::TextureFormat framebufferFormat,
                                             wgpu::ShaderModule vertexShader,
                                             wgpu::ShaderModule fragmentShader,
                                             EmJsHandle* pipelineJSHandleIfNeeded) override;

    wgpu::RenderPassEncoder makePLSRenderPass(wgpu::CommandEncoder,
                                              const PLSRenderTargetWebGPU*,
                                              wgpu::LoadOp,
                                              const wgpu::Color& clearColor,
                                              EmJsHandle* renderPassJSHandleIfNeeded) override;

private:
    friend class PLSRenderContextWebGPUImpl;

    PLSRenderContextWebGPUVulkan(wgpu::Device device,
                                 wgpu::Queue queue,
                                 const ContextOptions& contextOptions,
                                 const pls::PlatformFeatures& baselinePlatformFeatures) :
        PLSRenderContextWebGPUImpl(device, queue, contextOptions, baselinePlatformFeatures)
    {
        assert(contextOptions.pixelLocalStorageType == PixelLocalStorageType::subpassLoad);
    }

    EmJsHandle m_plsTextureBindGroupJSHandle;
};
} // namespace rive::pls

#endif
