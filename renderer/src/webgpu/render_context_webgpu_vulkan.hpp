/*
 * Copyright 2023 Rive
 */

#pragma once

#ifdef RIVE_WEBGPU

#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

namespace rive::gpu
{
// WebGPU implementation that uses Vulkan input attachments,
// VK_EXT_rasterization_order_attachment_access, and subpassLoad() for pixel
// local storage. These Vulkan features are accessed via nonstandard WebGPU
// APIs.
class RenderContextWebGPUVulkan : public RenderContextWebGPUImpl
{
public:
    rcp<RenderTargetWebGPU> makeRenderTarget(wgpu::TextureFormat,
                                             uint32_t width,
                                             uint32_t height) override;

protected:
    wgpu::BindGroupLayout initTextureBindGroup() override;

    wgpu::RenderPipeline makeDrawPipeline(
        rive::gpu::DrawType drawType,
        wgpu::TextureFormat framebufferFormat,
        wgpu::ShaderModule vertexShader,
        wgpu::ShaderModule fragmentShader,
        EmJsHandle* pipelineJSHandleIfNeeded) override;

    wgpu::RenderPassEncoder makePLSRenderPass(
        wgpu::CommandEncoder,
        const RenderTargetWebGPU*,
        wgpu::LoadOp,
        const wgpu::Color& clearColor,
        EmJsHandle* renderPassJSHandleIfNeeded) override;

private:
    friend class RenderContextWebGPUImpl;

    RenderContextWebGPUVulkan(wgpu::Device device,
                              wgpu::Queue queue,
                              const ContextOptions& contextOptions) :
        RenderContextWebGPUImpl(device, queue, contextOptions)
    {
        assert(contextOptions.plsType == PixelLocalStorageType::subpassLoad);
        m_platformFeatures.supportsRasterOrdering = true;
    }

    EmJsHandle m_plsTextureBindGroupJSHandle;
};
} // namespace rive::gpu

#endif
