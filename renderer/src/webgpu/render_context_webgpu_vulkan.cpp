/*
 * Copyright 2023 Rive
 */

#ifdef RIVE_WEBGPU

#include "render_context_webgpu_vulkan.hpp"

#include "shaders/constants.glsl"

#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>

namespace rive::gpu
{
// Create a group for binding PLS textures as Vulkan input attachments. The "inputTexture" property
// is nonstandard WebGPU.
EM_JS(int, make_pls_input_attachment_texture_bind_group, (int device), {
    device = JsValStore.get(device);
    const plsBindBroupLayout = device.createBindGroupLayout({
        entries : [
            {
                binding : 0,
                visibility : GPUShaderStage.FRAGMENT,
                inputTexture : {viewDimension : "2d"},
            },
            {
                binding : 1,
                visibility : GPUShaderStage.FRAGMENT,
                inputTexture : {viewDimension : "2d"},
            },
            {
                binding : 2,
                visibility : GPUShaderStage.FRAGMENT,
                inputTexture : {viewDimension : "2d"},
            },
            {
                binding : 3,
                visibility : GPUShaderStage.FRAGMENT,
                inputTexture : {viewDimension : "2d"},
            },
        ]
    });
    return JsValStore.add(plsBindBroupLayout);
});

wgpu::BindGroupLayout PLSRenderContextWebGPUVulkan::initPLSTextureBindGroup()
{
    m_plsTextureBindGroupJSHandle = EmJsHandle(make_pls_input_attachment_texture_bind_group(
        emscripten_webgpu_export_device(device().Get())));
    return wgpu::BindGroupLayout::Acquire(
        emscripten_webgpu_import_bind_group_layout(m_plsTextureBindGroupJSHandle.get()));
}

// The "TRANSIENT_ATTACHMENT" and "INPUT_ATTACHMENT" flags are nonstandard WebGPU.
EM_JS(int, texture_usage_transient_input_attachment, (), {
    return GPUTextureUsage.TRANSIENT_ATTACHMENT | GPUTextureUsage.INPUT_ATTACHMENT;
});

rcp<PLSRenderTargetWebGPU> PLSRenderContextWebGPUVulkan::makeRenderTarget(
    wgpu::TextureFormat framebufferFormat,
    uint32_t width,
    uint32_t height)
{
    return rcp(new PLSRenderTargetWebGPU(
        device(),
        framebufferFormat,
        width,
        height,
        static_cast<wgpu::TextureUsage>(texture_usage_transient_input_attachment())));
}

// Create a standard PLS "draw" pipeline that uses Vulkan input attachments and
// VK_EXT_rasterization_order_attachment_access for pixel local storage. The "inputs" and "features"
// properties on GPUFragmentState, and the "usedAsInput" property on GPUColorTargetState are
// nonstandard WebGPU.
EM_JS(int,
      make_pls_draw_pipeline,
      (int device,
       rive::gpu::DrawType drawType,
       wgpu::TextureFormat framebufferFormat,
       int vertexShader,
       int fragmentShader,
       int pipelineLayout,
       bool clockwiseFrontFace),
      {
          device = JsValStore.get(device);
          vertexShader = JsValStore.get(vertexShader);
          fragmentShader = JsValStore.get(fragmentShader);
          pipelineLayout = JsValStore.get(pipelineLayout);

          const RGBA8_UNORM = 0x00000012;
          const BGRA8_UNORM = 0x00000017;
          switch (framebufferFormat)
          {
              case RGBA8_UNORM:
                  framebufferFormat = "rgba8unorm";
                  break;
              case BGRA8_UNORM:
                  framebufferFormat = "bgra8unorm";
                  break;
              default:
                  throw "unsupported framebuffer format " + framebufferFormat;
          }

          let vertexBufferLayout;
          switch (drawType)
          {
              case 0:
              case 1:
                  attrs = [
                      {
                          format : "float32x4",
                          offset : 0,
                          shaderLocation : 0,
                      },
                      {
                          format : "float32x4",
                          offset : 4 * 4,
                          shaderLocation : 1,
                      },
                  ];
                  vertexBufferLayouts = [
                      {
                          arrayStride : 4 * 8,
                          stepMode : "vertex",
                          attributes : attrs,
                      },
                  ];
                  break;
              case 2:
                  attrs = [
                      {
                          format : "float32x3",
                          offset : 0,
                          shaderLocation : 0,
                      },
                  ];

                  vertexBufferLayouts = [
                      {
                          arrayStride : 4 * 3,
                          stepMode : "vertex",
                          attributes : attrs,
                      },
                  ];
                  break;
              case 3:
                  attrs = [
                      {
                          format : "float32x2",
                          offset : 0,
                          shaderLocation : 0,
                      },
                      {
                          format : "float32x2",
                          offset : 0,
                          shaderLocation : 1,
                      },
                  ];

                  vertexBufferLayouts = [
                      {
                          arrayStride : 4 * 2,
                          stepMode : "vertex",
                          attributes : [attrs[0]],
                      },
                      {
                          arrayStride : 4 * 2,
                          stepMode : "vertex",
                          attributes : [attrs[1]],
                      },
                  ];
                  break;
          }

          const pipelineDesc = {
              vertex : {
                  module : vertexShader,
                  entryPoint : "main",
                  buffers : vertexBufferLayouts,
              },
              fragment : {
                  module : fragmentShader,
                  entryPoint : "main",
                  inputs : [
                      {format : framebufferFormat, usedAsColor : true},
                      {format : "r32uint", usedAsColor : true},
                      {format : "r32uint", usedAsColor : true},
                      {format : framebufferFormat, usedAsColor : true},
                  ],
                  targets : [
                      {format : framebufferFormat, usedAsInput : true},
                      {format : "r32uint", usedAsInput : true},
                      {format : "r32uint", usedAsInput : true},
                      {format : framebufferFormat, usedAsInput : true},
                  ],
                  features : GPUFragmentStateFeatures.RASTERIZATION_ORDER_ATTACHMENT_ACCESS,
              },
              primitive : {
                  topology : "triangle-list",
                  frontFace : clockwiseFrontFace ? "cw" : "ccw",
                  cullMode : drawType != 3 ? "back" : "none"
              },
              layout : pipelineLayout
          };

          const pipeline = device.createRenderPipeline(pipelineDesc);
          return JsValStore.add(pipeline);
      });

wgpu::RenderPipeline PLSRenderContextWebGPUVulkan::makePLSDrawPipeline(
    rive::gpu::DrawType drawType,
    wgpu::TextureFormat framebufferFormat,
    wgpu::ShaderModule vertexShader,
    wgpu::ShaderModule fragmentShader,
    EmJsHandle* pipelineJSHandleIfNeeded)
{
    *pipelineJSHandleIfNeeded = EmJsHandle(
        make_pls_draw_pipeline(emscripten_webgpu_export_device(device().Get()),
                               drawType,
                               framebufferFormat,
                               emscripten_webgpu_export_shader_module(vertexShader.Get()),
                               emscripten_webgpu_export_shader_module(fragmentShader.Get()),
                               emscripten_webgpu_export_pipeline_layout(drawPipelineLayout().Get()),
                               frontFaceForOnScreenDraws() == wgpu::FrontFace::CW));
    return wgpu::RenderPipeline::Acquire(
        emscripten_webgpu_import_render_pipeline(pipelineJSHandleIfNeeded->get()));
}

// Create a standard PLS "draw" render pass that uses Vulkan input attachments for pixel local
// storage. The "inputAttachments" property on GPURenderPassDescriptor is nonstandard WebGPU.
EM_JS(int,
      make_pls_render_pass,
      (int encoder,
       int tex0,
       int tex1,
       int tex2,
       int tex3,
       wgpu::LoadOp loadOp,
       double r,
       double g,
       double b,
       double a),
      {
          encoder = JsValStore.get(encoder);
          tex0 = JsValStore.get(tex0);
          tex1 = JsValStore.get(tex1);
          tex2 = JsValStore.get(tex2);
          tex3 = JsValStore.get(tex3);

          const CLEAR = 1;
          const LOAD = 2;
          switch (loadOp)
          {
              case CLEAR:
                  loadOp = "clear";
                  break;
              case LOAD:
                  loadOp = "load";
                  break;
              default:
                  throw "unsupported framebuffer format " + framebufferFormat;
          }

          const zero = {r : 0, g : 0, b : 0, a : 0};
          const plsAttachments = [
              {
                  view : tex0,
                  loadOp : loadOp,
                  storeOp : "store",
                  clearValue : {r : r, g : g, b : b, a : a},
              },
              {
                  view : tex1,
                  loadOp : "clear",
                  storeOp : "discard",
                  clearValue : zero,
              },
              {
                  view : tex2,
                  loadOp : "clear",
                  storeOp : "discard",
                  clearValue : zero,
              },
              {
                  view : tex3,
                  loadOp : "clear",
                  storeOp : "discard",
                  clearValue : zero,
              },
          ];

          const renderPass = encoder.beginRenderPass({
              inputAttachments : plsAttachments,
              colorAttachments : plsAttachments,
          });

          return JsValStore.add(renderPass);
      });

wgpu::RenderPassEncoder PLSRenderContextWebGPUVulkan::makePLSRenderPass(
    wgpu::CommandEncoder encoder,
    const PLSRenderTargetWebGPU* renderTarget,
    wgpu::LoadOp loadOp,
    const wgpu::Color& clearColor,
    EmJsHandle* renderPassJSHandleIfNeeded)
{
    *renderPassJSHandleIfNeeded = EmJsHandle(make_pls_render_pass(
        emscripten_webgpu_export_command_encoder(encoder.Get()),
        emscripten_webgpu_export_texture_view(renderTarget->m_targetTextureView.Get()),
        emscripten_webgpu_export_texture_view(renderTarget->m_coverageTextureView.Get()),
        emscripten_webgpu_export_texture_view(renderTarget->m_clipTextureView.Get()),
        emscripten_webgpu_export_texture_view(renderTarget->m_scratchColorTextureView.Get()),
        loadOp,
        clearColor.r,
        clearColor.g,
        clearColor.b,
        clearColor.a));
    return wgpu::RenderPassEncoder::Acquire(
        emscripten_webgpu_import_render_pass_encoder(renderPassJSHandleIfNeeded->get()));
}
} // namespace rive::gpu

#endif
