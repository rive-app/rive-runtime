/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#include "rive/renderer/image.hpp"
#include "shaders/constants.glsl"

#include "generated/shaders/spirv/color_ramp.vert.h"
#include "generated/shaders/spirv/color_ramp.frag.h"
#include "generated/shaders/spirv/tessellate.vert.h"
#include "generated/shaders/spirv/tessellate.frag.h"
#include "generated/shaders/spirv/draw_path.vert.h"
#include "generated/shaders/spirv/draw_path.frag.h"
#include "generated/shaders/spirv/draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_image_mesh.frag.h"

#include "generated/shaders/glsl.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"
#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/draw_path.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_image_mesh.glsl.hpp"

#include <sstream>
#include <string>

#ifdef RIVE_DAWN
#include <dawn/webgpu_cpp.h>

static void enable_shader_pixel_local_storage_ext(wgpu::RenderPassEncoder, bool enabled)
{
    RIVE_UNREACHABLE();
}

static void write_texture(wgpu::Queue queue,
                          wgpu::Texture texture,
                          uint32_t bytesPerRow,
                          uint32_t width,
                          uint32_t height,
                          const void* data,
                          size_t dataSize)
{
    wgpu::ImageCopyTexture dest = {
        .texture = texture,
        .mipLevel = 0,
    };
    wgpu::TextureDataLayout layout = {
        .bytesPerRow = bytesPerRow,
    };
    wgpu::Extent3D extent = {
        .width = width,
        .height = height,
    };
    queue.WriteTexture(&dest, data, dataSize, &layout, &extent);
}

static void write_buffer(wgpu::Queue queue, wgpu::Buffer buffer, const void* data, size_t dataSize)
{
    queue.WriteBuffer(buffer, 0, data, dataSize);
}
#endif

#ifdef RIVE_WEBGPU
#include "render_context_webgpu_vulkan.hpp"
#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>

EM_JS(void, enable_shader_pixel_local_storage_ext_js, (int renderPass, bool enabled), {
    renderPass = JsValStore.get(renderPass);
    renderPass.setShaderPixelLocalStorageEnabled(Boolean(enabled));
});

static void enable_shader_pixel_local_storage_ext(wgpu::RenderPassEncoder renderPass, bool enabled)
{
    enable_shader_pixel_local_storage_ext_js(
        emscripten_webgpu_export_render_pass_encoder(renderPass.Get()),
        enabled);
}

EM_JS(void,
      write_texture_js,
      (int queue,
       int texture,
       uint32_t bytesPerRow,
       uint32_t width,
       uint32_t height,
       uintptr_t indexU8,
       size_t dataSize),
      {
          queue = JsValStore.get(queue);
          texture = JsValStore.get(texture);
          // Copy data off the WASM heap before sending it to WebGPU bindings.
          const data = new Uint8Array(dataSize);
          data.set(Module.HEAPU8.subarray(indexU8, indexU8 + dataSize));
          queue.writeTexture({texture},
                             data,
                             {bytesPerRow : bytesPerRow},
                             {width : width, height : height});
      });

static void write_texture(wgpu::Queue queue,
                          wgpu::Texture texture,
                          uint32_t bytesPerRow,
                          uint32_t width,
                          uint32_t height,
                          const void* data,
                          size_t dataSize)
{
    write_texture_js(emscripten_webgpu_export_queue(queue.Get()),
                     emscripten_webgpu_export_texture(texture.Get()),
                     bytesPerRow,
                     width,
                     height,
                     reinterpret_cast<uintptr_t>(data),
                     dataSize);
}

EM_JS(void, write_buffer_js, (int queue, int buffer, uintptr_t indexU8, size_t dataSize), {
    queue = JsValStore.get(queue);
    buffer = JsValStore.get(buffer);
    // Copy data off the WASM heap before sending it to WebGPU bindings.
    const data = new Uint8Array(dataSize);
    data.set(Module.HEAPU8.subarray(indexU8, indexU8 + dataSize));
    queue.writeBuffer(buffer, 0, data, 0, dataSize);
});

static void write_buffer(wgpu::Queue queue, wgpu::Buffer buffer, const void* data, size_t dataSize)
{
    write_buffer_js(emscripten_webgpu_export_queue(queue.Get()),
                    emscripten_webgpu_export_buffer(buffer.Get()),
                    reinterpret_cast<uintptr_t>(data),
                    dataSize);
}
#endif

namespace rive::gpu
{
// Draws emulated render-pass load/store actions for EXT_shader_pixel_local_storage.
class PLSRenderContextWebGPUImpl::LoadStoreEXTPipeline
{
public:
    LoadStoreEXTPipeline(PLSRenderContextWebGPUImpl* context,
                         LoadStoreActionsEXT actions,
                         wgpu::TextureFormat framebufferFormat) :
        m_framebufferFormat(framebufferFormat)
    {
        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
        if (actions & LoadStoreActionsEXT::clearColor)
        {
            // Create a uniform buffer binding for the clear color.
            wgpu::BindGroupLayoutEntry bindingLayouts[] = {
                {
                    .binding = 0,
                    .visibility = wgpu::ShaderStage::Fragment,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::Uniform,
                        },
                },
            };

            wgpu::BindGroupLayoutDescriptor bindingsDesc = {
                .entryCount = std::size(bindingLayouts),
                .entries = bindingLayouts,
            };

            m_bindGroupLayout = context->m_device.CreateBindGroupLayout(&bindingsDesc);

            pipelineLayoutDesc = {
                .bindGroupLayoutCount = 1,
                .bindGroupLayouts = &m_bindGroupLayout,
            };
        }
        else
        {
            pipelineLayoutDesc = {
                .bindGroupLayoutCount = 0,
                .bindGroupLayouts = nullptr,
            };
        }

        wgpu::PipelineLayout pipelineLayout =
            context->m_device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule fragmentShader;
        std::ostringstream glsl;
        glsl << "#version 310 es\n";
        glsl << "#pragma shader_stage(fragment)\n";
        glsl << "#define " GLSL_FRAGMENT "\n";
        glsl << "#define " GLSL_ENABLE_CLIPPING "\n";
        BuildLoadStoreEXTGLSL(glsl, actions);
        fragmentShader = m_fragmentShaderHandle.compileShaderModule(context->m_device,
                                                                    glsl.str().c_str(),
                                                                    "glsl-raw");

        wgpu::ColorTargetState colorTargetState = {
            .format = framebufferFormat,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = context->m_loadStoreEXTVertexShader,
                    .entryPoint = "main",
                    .bufferCount = 0,
                    .buffers = nullptr,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleStrip,
                    .frontFace = context->m_frontFaceForOnScreenDraws,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = context->m_device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const
    {
        assert(m_bindGroupLayout); // We only have a bind group if there is a clear color.
        return m_bindGroupLayout;
    }

    wgpu::RenderPipeline renderPipeline(wgpu::TextureFormat framebufferFormat) const
    {
        assert(framebufferFormat == m_framebufferFormat);
        return m_renderPipeline;
    }

private:
    const wgpu::TextureFormat m_framebufferFormat RIVE_MAYBE_UNUSED;
    wgpu::BindGroupLayout m_bindGroupLayout;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipeline;
};

// Renders color ramps to the gradient texture.
class PLSRenderContextWebGPUImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(wgpu::Device device)
    {
        wgpu::BindGroupLayoutEntry bindingLayouts[] = {
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::Uniform,
                    },
            },
        };

        wgpu::BindGroupLayoutDescriptor bindingsDesc = {
            .entryCount = std::size(bindingLayouts),
            .entries = bindingLayouts,
        };

        m_bindGroupLayout = device.CreateBindGroupLayout(&bindingsDesc);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &m_bindGroupLayout,
        };

        wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader =
            m_vertexShaderHandle.compileSPIRVShaderModule(device,
                                                          color_ramp_vert,
                                                          std::size(color_ramp_vert));

        wgpu::VertexAttribute attrs[] = {
            {
                .format = wgpu::VertexFormat::Uint32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
        };

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .arrayStride = sizeof(gpu::GradientSpan),
            .stepMode = wgpu::VertexStepMode::Instance,
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };

        wgpu::ShaderModule fragmentShader =
            m_fragmentShaderHandle.compileSPIRVShaderModule(device,
                                                            color_ramp_frag,
                                                            std::size(color_ramp_frag));

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::RGBA8Unorm,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShader,
                    .entryPoint = "main",
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleStrip,
                    .frontFace = kFrontFaceForOffscreenDraws,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const { return m_bindGroupLayout; }
    const wgpu::RenderPipeline& renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_bindGroupLayout;
    EmJsHandle m_vertexShaderHandle;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class PLSRenderContextWebGPUImpl::TessellatePipeline
{
public:
    TessellatePipeline(wgpu::Device device, const ContextOptions& contextOptions)
    {
        wgpu::BindGroupLayoutEntry bindingLayouts[] = {
            contextOptions.disableStorageBuffers ?
                wgpu::BindGroupLayoutEntry{
                    .binding = PATH_BUFFER_IDX,
                    .visibility = wgpu::ShaderStage::Vertex,
                    .texture =
                        {
                            .sampleType = wgpu::TextureSampleType::Uint,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                } :
                wgpu::BindGroupLayoutEntry{
                    .binding = PATH_BUFFER_IDX,
                    .visibility = wgpu::ShaderStage::Vertex,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::ReadOnlyStorage,
                        },
                },
            contextOptions.disableStorageBuffers ?
                wgpu::BindGroupLayoutEntry{
                    .binding = CONTOUR_BUFFER_IDX,
                    .visibility = wgpu::ShaderStage::Vertex,
                    .texture =
                        {
                            .sampleType = wgpu::TextureSampleType::Uint,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                } :
                wgpu::BindGroupLayoutEntry{
                    .binding = CONTOUR_BUFFER_IDX,
                    .visibility = wgpu::ShaderStage::Vertex,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::ReadOnlyStorage,
                        },
                },
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::Uniform,
                    },
            },
        };

        wgpu::BindGroupLayoutDescriptor bindingsDesc = {
            .entryCount = std::size(bindingLayouts),
            .entries = bindingLayouts,
        };

        m_bindGroupLayout = device.CreateBindGroupLayout(&bindingsDesc);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &m_bindGroupLayout,
        };

        wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader;
        if (contextOptions.disableStorageBuffers)
        {
            // The built-in SPIRV does not #define DISABLE_SHADER_STORAGE_BUFFERS. Recompile the
            // tessellation shader with storage buffers disabled.
            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 460\n";
            vertexGLSL << "#pragma shader_stage(vertex)\n";
            vertexGLSL << "#define " GLSL_VERTEX "\n";
            vertexGLSL << "#define " GLSL_DISABLE_SHADER_STORAGE_BUFFERS "\n";
            vertexGLSL << "#define " GLSL_TARGET_VULKAN "\n";
            vertexGLSL << "#extension GL_EXT_samplerless_texture_functions : enable\n";
            vertexGLSL << glsl::glsl << "\n";
            vertexGLSL << glsl::constants << "\n";
            vertexGLSL << glsl::common << "\n";
            vertexGLSL << glsl::tessellate << "\n";
            vertexShader =
                m_vertexShaderHandle.compileShaderModule(device, vertexGLSL.str().c_str(), "glsl");
        }
        else
        {
            vertexShader =
                m_vertexShaderHandle.compileSPIRVShaderModule(device,
                                                              tessellate_vert,
                                                              std::size(tessellate_vert));
        }

        wgpu::VertexAttribute attrs[] = {
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 4 * sizeof(float),
                .shaderLocation = 1,
            },
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 8 * sizeof(float),
                .shaderLocation = 2,
            },
            {
                .format = wgpu::VertexFormat::Uint32x4,
                .offset = 12 * sizeof(float),
                .shaderLocation = 3,
            },
        };

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .arrayStride = sizeof(gpu::TessVertexSpan),
            .stepMode = wgpu::VertexStepMode::Instance,
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };

        wgpu::ShaderModule fragmentShader =
            m_fragmentShaderHandle.compileSPIRVShaderModule(device,
                                                            tessellate_frag,
                                                            std::size(tessellate_frag));

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::RGBA32Uint,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShader,
                    .entryPoint = "main",
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleList,
                    .frontFace = kFrontFaceForOffscreenDraws,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const { return m_bindGroupLayout; }
    const wgpu::RenderPipeline renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_bindGroupLayout;
    EmJsHandle m_vertexShaderHandle;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipeline;
};

// Draw paths and image meshes using the gradient and tessellation textures.
class PLSRenderContextWebGPUImpl::DrawPipeline
{
public:
    DrawPipeline(PLSRenderContextWebGPUImpl* context,
                 DrawType drawType,
                 gpu::ShaderFeatures shaderFeatures,
                 const ContextOptions& contextOptions)
    {
        PixelLocalStorageType plsType = context->m_contextOptions.plsType;
        wgpu::ShaderModule vertexShader, fragmentShader;
        if (plsType == PixelLocalStorageType::subpassLoad ||
            plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage ||
            contextOptions.disableStorageBuffers)
        {
            const char* language;
            const char* versionString;
            if (plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
            {
                language = "glsl-raw";
                versionString = "#version 310 es";
            }
            else
            {
                language = "glsl";
                versionString = "#version 460";
            }

            std::ostringstream glsl;
            auto addDefine = [&glsl](const char* name) { glsl << "#define " << name << "\n"; };
            if (plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
            {
                glsl << "#ifdef GL_EXT_shader_pixel_local_storage\n";
                addDefine(GLSL_PLS_IMPL_EXT_NATIVE);
                glsl << "#else\n";
                glsl << "#extension GL_EXT_samplerless_texture_functions : enable\n";
                addDefine(GLSL_TARGET_VULKAN);
                // If we are being compiled by SPIRV transpiler for introspection,
                // GL_EXT_shader_pixel_local_storage will not be defined.
                addDefine(GLSL_PLS_IMPL_NONE);
                glsl << "#endif\n";
            }
            else
            {
                glsl << "#extension GL_EXT_samplerless_texture_functions : enable\n";
                addDefine(GLSL_TARGET_VULKAN);
                addDefine(plsType == PixelLocalStorageType::subpassLoad ? GLSL_PLS_IMPL_SUBPASS_LOAD
                                                                        : GLSL_PLS_IMPL_NONE);
            }
            if (contextOptions.disableStorageBuffers)
            {
                addDefine(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
            }
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    addDefine(GLSL_ENABLE_INSTANCE_INDEX);
                    if (plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
                    {
                        // The WebGPU layer automatically searches for a uniform named
                        // "SPIRV_Cross_BaseInstance" and manages it for us.
                        addDefine(GLSL_ENABLE_SPIRV_CROSS_BASE_INSTANCE);
                    }
                    break;
                case DrawType::interiorTriangulation:
                    addDefine(GLSL_DRAW_INTERIOR_TRIANGLES);
                    break;
                case DrawType::imageRect:
                    RIVE_UNREACHABLE();
                case DrawType::imageMesh:
                    break;
                case DrawType::gpuAtomicInitialize:
                case DrawType::gpuAtomicResolve:
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
            }
            for (size_t i = 0; i < gpu::kShaderFeatureCount; ++i)
            {
                ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
                if (shaderFeatures & feature)
                {
                    addDefine(GetShaderFeatureGLSLName(feature));
                }
            }
            glsl << gpu::glsl::glsl << '\n';
            glsl << gpu::glsl::constants << '\n';
            glsl << gpu::glsl::common << '\n';
            if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
            {
                glsl << gpu::glsl::advanced_blend << '\n';
            }
            if (context->platformFeatures().avoidFlatVaryings)
            {
                addDefine(GLSL_OPTIONALLY_FLAT);
            }
            else
            {
                glsl << "#define " GLSL_OPTIONALLY_FLAT " flat\n";
            }
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    addDefine(GLSL_DRAW_PATH);
                    glsl << gpu::glsl::draw_path_common << '\n';
                    glsl << gpu::glsl::draw_path << '\n';
                    break;
                case DrawType::interiorTriangulation:
                    addDefine(GLSL_DRAW_INTERIOR_TRIANGLES);
                    glsl << gpu::glsl::draw_path_common << '\n';
                    glsl << gpu::glsl::draw_path << '\n';
                    break;
                case DrawType::imageRect:
                    addDefine(GLSL_DRAW_IMAGE);
                    addDefine(GLSL_DRAW_IMAGE_RECT);
                    RIVE_UNREACHABLE();
                case DrawType::imageMesh:
                    addDefine(GLSL_DRAW_IMAGE);
                    addDefine(GLSL_DRAW_IMAGE_MESH);
                    glsl << gpu::glsl::draw_image_mesh << '\n';
                    break;
                case DrawType::gpuAtomicInitialize:
                    addDefine(GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS);
                    addDefine(GLSL_INITIALIZE_PLS);
                    RIVE_UNREACHABLE();
                case DrawType::gpuAtomicResolve:
                    addDefine(GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS);
                    addDefine(GLSL_RESOLVE_PLS);
                    RIVE_UNREACHABLE();
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
            }

            std::ostringstream vertexGLSL;
            vertexGLSL << versionString << "\n";
            vertexGLSL << "#pragma shader_stage(vertex)\n";
            vertexGLSL << "#define " GLSL_VERTEX "\n";
            vertexGLSL << glsl.str();
            vertexShader = m_vertexShaderHandle.compileShaderModule(context->m_device,
                                                                    vertexGLSL.str().c_str(),
                                                                    language);

            std::ostringstream fragmentGLSL;
            fragmentGLSL << versionString << "\n";
            fragmentGLSL << "#pragma shader_stage(fragment)\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT "\n";
            fragmentGLSL << glsl.str();
            fragmentShader = m_fragmentShaderHandle.compileShaderModule(context->m_device,
                                                                        fragmentGLSL.str().c_str(),
                                                                        language);
        }
        else
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    vertexShader =
                        m_vertexShaderHandle.compileSPIRVShaderModule(context->m_device,
                                                                      draw_path_vert,
                                                                      std::size(draw_path_vert));
                    fragmentShader =
                        m_fragmentShaderHandle.compileSPIRVShaderModule(context->m_device,
                                                                        draw_path_frag,
                                                                        std::size(draw_path_frag));
                    break;
                case DrawType::interiorTriangulation:
                    vertexShader = m_vertexShaderHandle.compileSPIRVShaderModule(
                        context->m_device,
                        draw_interior_triangles_vert,
                        std::size(draw_interior_triangles_vert));
                    fragmentShader = m_fragmentShaderHandle.compileSPIRVShaderModule(
                        context->m_device,
                        draw_interior_triangles_frag,
                        std::size(draw_interior_triangles_frag));
                    break;
                case DrawType::imageRect:
                    RIVE_UNREACHABLE();
                case DrawType::imageMesh:
                    vertexShader = m_vertexShaderHandle.compileSPIRVShaderModule(
                        context->m_device,
                        draw_image_mesh_vert,
                        std::size(draw_image_mesh_vert));
                    fragmentShader = m_fragmentShaderHandle.compileSPIRVShaderModule(
                        context->m_device,
                        draw_image_mesh_frag,
                        std::size(draw_image_mesh_frag));
                    break;
                case DrawType::gpuAtomicInitialize:
                case DrawType::gpuAtomicResolve:
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
            }
        }

        for (auto framebufferFormat :
             {wgpu::TextureFormat::BGRA8Unorm, wgpu::TextureFormat::RGBA8Unorm})
        {
            int pipelineIdx = RenderPipelineIdx(framebufferFormat);
            m_renderPipelines[pipelineIdx] =
                context->makePLSDrawPipeline(drawType,
                                             framebufferFormat,
                                             vertexShader,
                                             fragmentShader,
                                             &m_renderPipelineHandles[pipelineIdx]);
        }
    }

    const wgpu::RenderPipeline renderPipeline(wgpu::TextureFormat framebufferFormat) const
    {
        return m_renderPipelines[RenderPipelineIdx(framebufferFormat)];
    }

private:
    static int RenderPipelineIdx(wgpu::TextureFormat framebufferFormat)
    {
        assert(framebufferFormat == wgpu::TextureFormat::BGRA8Unorm ||
               framebufferFormat == wgpu::TextureFormat::RGBA8Unorm);
        return framebufferFormat == wgpu::TextureFormat::BGRA8Unorm ? 1 : 0;
    }

    EmJsHandle m_vertexShaderHandle;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipelines[2];
    EmJsHandle m_renderPipelineHandles[2];
};

PLSRenderContextWebGPUImpl::PLSRenderContextWebGPUImpl(
    wgpu::Device device,
    wgpu::Queue queue,
    const ContextOptions& contextOptions,
    const PlatformFeatures& baselinePlatformFeatures) :
    m_device(device),
    m_queue(queue),
    m_contextOptions(contextOptions),
    m_frontFaceForOnScreenDraws(wgpu::FrontFace::CW),
    m_colorRampPipeline(std::make_unique<ColorRampPipeline>(m_device)),
    m_tessellatePipeline(std::make_unique<TessellatePipeline>(m_device, m_contextOptions))
{
    m_platformFeatures = baselinePlatformFeatures;
    m_platformFeatures.invertOffscreenY = true;

    if (m_contextOptions.plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage &&
        baselinePlatformFeatures.uninvertOnScreenY)
    {
        // We will use "glsl-raw" in order to access EXT_shader_pixel_local_storage, so the WebGPU
        // layer won't actually get a chance to negate Y like it thinks it will.
        m_platformFeatures.uninvertOnScreenY = false;
        // PLS always expects CW, but in this case, we need to specify CCW. This is because the
        // WebGPU layer thinks it's going to negate Y in our shader, and will therefore also flip
        // our frontFace. However, since we will use raw-glsl shaders, the WebGPU layer won't
        // actually get a chance to negate Y like it thinks it will. Therefore, we emit the wrong
        // frontFace, in anticipation of it getting flipped into the correct frontFace on its way to
        // the driver.
        m_frontFaceForOnScreenDraws = wgpu::FrontFace::CCW;
    }
}

void PLSRenderContextWebGPUImpl::initGPUObjects()
{
    wgpu::BindGroupLayoutEntry perFlushBindingLayouts[] = {
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Vertex,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Uint,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = PATH_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
            wgpu::BindGroupLayoutEntry{
                .binding = PATH_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
            wgpu::BindGroupLayoutEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                },
        },
        {
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = true,
                    .minBindingSize = sizeof(gpu::ImageDrawUniforms),
                },
        },
    };

    wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
        .entryCount = std::size(perFlushBindingLayouts),
        .entries = perFlushBindingLayouts,
    };

    m_drawBindGroupLayouts[PER_FLUSH_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&perFlushBindingsDesc);

    wgpu::BindGroupLayoutEntry perDrawBindingLayouts[] = {
        {
            .binding = IMAGE_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
    };

    wgpu::BindGroupLayoutDescriptor perDrawBindingsDesc = {
        .entryCount = std::size(perDrawBindingLayouts),
        .entries = perDrawBindingLayouts,
    };

    m_drawBindGroupLayouts[PER_DRAW_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&perDrawBindingsDesc);

    wgpu::BindGroupLayoutEntry drawBindingSamplerLayouts[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
        {
            .binding = IMAGE_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
    };

    wgpu::BindGroupLayoutDescriptor samplerBindingsDesc = {
        .entryCount = std::size(drawBindingSamplerLayouts),
        .entries = drawBindingSamplerLayouts,
    };

    m_drawBindGroupLayouts[SAMPLER_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&samplerBindingsDesc);

    wgpu::SamplerDescriptor linearSamplerDesc = {
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Nearest,
    };

    m_linearSampler = m_device.CreateSampler(&linearSamplerDesc);

    wgpu::SamplerDescriptor mipmapSamplerDesc = {
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Nearest,
    };

    m_mipmapSampler = m_device.CreateSampler(&mipmapSamplerDesc);

    wgpu::BindGroupEntry samplerBindingEntries[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .sampler = m_linearSampler,
        },
        {
            .binding = IMAGE_TEXTURE_IDX,
            .sampler = m_mipmapSampler,
        },
    };

    wgpu::BindGroupDescriptor samplerBindGroupDesc = {
        .layout = m_drawBindGroupLayouts[SAMPLER_BINDINGS_SET],
        .entryCount = std::size(samplerBindingEntries),
        .entries = samplerBindingEntries,
    };

    m_samplerBindings = m_device.CreateBindGroup(&samplerBindGroupDesc);

    bool needsPLSTextureBindings = m_contextOptions.plsType == PixelLocalStorageType::subpassLoad;
    if (needsPLSTextureBindings)
    {
        m_drawBindGroupLayouts[PLS_TEXTURE_BINDINGS_SET] = initPLSTextureBindGroup();
    }

    wgpu::PipelineLayoutDescriptor drawPipelineLayoutDesc = {
        .bindGroupLayoutCount = static_cast<size_t>(
            needsPLSTextureBindings ? BINDINGS_SET_COUNT : BINDINGS_SET_COUNT - 1),
        .bindGroupLayouts = m_drawBindGroupLayouts,
    };

    m_drawPipelineLayout = m_device.CreatePipelineLayout(&drawPipelineLayoutDesc);

    if (m_contextOptions.plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
    {
        // We have to manually implement load/store operations from a shader when using
        // EXT_shader_pixel_local_storage.
        std::ostringstream glsl;
        glsl << "#version 310 es\n";
        glsl << "#pragma shader_stage(vertex)\n";
        glsl << "#define " GLSL_VERTEX "\n";
        // If we are being compiled by SPIRV transpiler for introspection, use gl_VertexIndex
        // instead of gl_VertexID.
        glsl << "#ifndef GL_EXT_shader_pixel_local_storage\n";
        glsl << "#define gl_VertexID gl_VertexIndex\n";
        glsl << "#endif\n";
        glsl << "#define " GLSL_ENABLE_CLIPPING "\n";
        BuildLoadStoreEXTGLSL(glsl, LoadStoreActionsEXT::none);
        m_loadStoreEXTVertexShader =
            m_loadStoreEXTVertexShaderHandle.compileShaderModule(m_device,
                                                                 glsl.str().c_str(),
                                                                 "glsl-raw");
        m_loadStoreEXTUniforms = makeUniformBufferRing(sizeof(float) * 4);
    }

    wgpu::BufferDescriptor tessSpanIndexBufferDesc = {
        .usage = wgpu::BufferUsage::Index,
        .size = sizeof(gpu::kTessSpanIndices),
        .mappedAtCreation = true,
    };
    m_tessSpanIndexBuffer = m_device.CreateBuffer(&tessSpanIndexBufferDesc);
    memcpy(m_tessSpanIndexBuffer.GetMappedRange(),
           gpu::kTessSpanIndices,
           sizeof(gpu::kTessSpanIndices));
    m_tessSpanIndexBuffer.Unmap();

    wgpu::BufferDescriptor patchBufferDesc = {
        .usage = wgpu::BufferUsage::Vertex,
        .size = kPatchVertexBufferCount * sizeof(PatchVertex),
        .mappedAtCreation = true,
    };
    m_pathPatchVertexBuffer = m_device.CreateBuffer(&patchBufferDesc);

    patchBufferDesc.size = (kPatchIndexBufferCount * sizeof(uint16_t));
    // WebGPU buffer sizes must be multiples of 4.
    patchBufferDesc.size = math::round_up_to_multiple_of<4>(patchBufferDesc.size);
    patchBufferDesc.usage = wgpu::BufferUsage::Index;
    m_pathPatchIndexBuffer = m_device.CreateBuffer(&patchBufferDesc);

    GeneratePatchBufferData(
        reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer.GetMappedRange()),
        reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer.GetMappedRange()));

    m_pathPatchVertexBuffer.Unmap();
    m_pathPatchIndexBuffer.Unmap();

    wgpu::TextureDescriptor nullImagePaintTextureDesc = {
        .usage = wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {1, 1},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    m_nullImagePaintTexture = m_device.CreateTexture(&nullImagePaintTextureDesc);
    m_nullImagePaintTextureView = m_nullImagePaintTexture.CreateView();
}

PLSRenderContextWebGPUImpl::~PLSRenderContextWebGPUImpl() {}

PLSRenderTargetWebGPU::PLSRenderTargetWebGPU(wgpu::Device device,
                                             wgpu::TextureFormat framebufferFormat,
                                             uint32_t width,
                                             uint32_t height,
                                             wgpu::TextureUsage additionalTextureFlags) :
    PLSRenderTarget(width, height), m_framebufferFormat(framebufferFormat)
{
    wgpu::TextureDescriptor desc = {
        .usage = wgpu::TextureUsage::RenderAttachment | additionalTextureFlags,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
    };

    desc.format = wgpu::TextureFormat::R32Uint;
    m_coverageTexture = device.CreateTexture(&desc);
    m_clipTexture = device.CreateTexture(&desc);

    desc.format = m_framebufferFormat;
    m_scratchColorTexture = device.CreateTexture(&desc);

    m_targetTextureView = {}; // Will be configured later by setTargetTexture().
    m_coverageTextureView = m_coverageTexture.CreateView();
    m_clipTextureView = m_clipTexture.CreateView();
    m_scratchColorTextureView = m_scratchColorTexture.CreateView();
}

void PLSRenderTargetWebGPU::setTargetTextureView(wgpu::TextureView textureView)
{
    m_targetTextureView = textureView;
}

rcp<PLSRenderTargetWebGPU> PLSRenderContextWebGPUImpl::makeRenderTarget(
    wgpu::TextureFormat framebufferFormat,
    uint32_t width,
    uint32_t height)
{
    return rcp(new PLSRenderTargetWebGPU(m_device,
                                         framebufferFormat,
                                         width,
                                         height,
                                         wgpu::TextureUsage::None));
}

class RenderBufferWebGPUImpl : public RenderBuffer
{
public:
    RenderBufferWebGPUImpl(wgpu::Device device,
                           wgpu::Queue queue,
                           RenderBufferType renderBufferType,
                           RenderBufferFlags renderBufferFlags,
                           size_t sizeInBytes) :
        RenderBuffer(renderBufferType, renderBufferFlags, sizeInBytes),
        m_device(device),
        m_queue(queue)
    {
        bool mappedOnceAtInitialization = flags() & RenderBufferFlags::mappedOnceAtInitialization;
        int bufferCount = mappedOnceAtInitialization ? 1 : gpu::kBufferRingSize;
        wgpu::BufferDescriptor desc = {
            .usage = type() == RenderBufferType::index ? wgpu::BufferUsage::Index
                                                       : wgpu::BufferUsage::Vertex,
            // WebGPU buffer sizes must be multiples of 4.
            .size = math::round_up_to_multiple_of<4>(sizeInBytes),
            .mappedAtCreation = mappedOnceAtInitialization,
        };
        if (!mappedOnceAtInitialization)
        {
            desc.usage |= wgpu::BufferUsage::CopyDst;
        }
        for (int i = 0; i < bufferCount; ++i)
        {
            m_buffers[i] = device.CreateBuffer(&desc);
        }
    }

    wgpu::Buffer submittedBuffer() const { return m_buffers[m_submittedBufferIdx]; }

protected:
    void* onMap() override
    {
        m_submittedBufferIdx = (m_submittedBufferIdx + 1) % gpu::kBufferRingSize;
        assert(m_buffers[m_submittedBufferIdx] != nullptr);
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            return m_buffers[m_submittedBufferIdx].GetMappedRange();
        }
        else
        {
            if (m_stagingBuffer == nullptr)
            {
                m_stagingBuffer.reset(new uint8_t[sizeInBytes()]);
            }
            return m_stagingBuffer.get();
        }
    }

    void onUnmap() override
    {
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            m_buffers[m_submittedBufferIdx].Unmap();
        }
        else
        {
            write_buffer(m_queue,
                         m_buffers[m_submittedBufferIdx],
                         m_stagingBuffer.get(),
                         sizeInBytes());
        }
    }

private:
    const wgpu::Device m_device;
    const wgpu::Queue m_queue;
    wgpu::Buffer m_buffers[gpu::kBufferRingSize];
    int m_submittedBufferIdx = -1;
    std::unique_ptr<uint8_t[]> m_stagingBuffer;
};

rcp<RenderBuffer> PLSRenderContextWebGPUImpl::makeRenderBuffer(RenderBufferType type,
                                                               RenderBufferFlags flags,
                                                               size_t sizeInBytes)
{
    return make_rcp<RenderBufferWebGPUImpl>(m_device, m_queue, type, flags, sizeInBytes);
}

class PLSTextureWebGPUImpl : public PLSTexture
{
public:
    PLSTextureWebGPUImpl(wgpu::Device device,
                         wgpu::Queue queue,
                         uint32_t width,
                         uint32_t height,
                         uint32_t mipLevelCount,
                         const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height)
    {
        wgpu::TextureDescriptor desc = {
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size = {width, height},
            .format = wgpu::TextureFormat::RGBA8Unorm,
            // TODO: implement mipmap generation.
            .mipLevelCount = 1, // mipLevelCount,
        };

        m_texture = device.CreateTexture(&desc);
        m_textureView = m_texture.CreateView();

        // Specify the top-level image in the mipmap chain.
        // TODO: implement mipmap generation.
        write_texture(queue,
                      m_texture,
                      width * 4,
                      width,
                      height,
                      imageDataRGBA,
                      height * width * 4);
    }

    wgpu::TextureView textureView() const { return m_textureView; }

private:
    wgpu::Texture m_texture;
    wgpu::TextureView m_textureView;
};

rcp<PLSTexture> PLSRenderContextWebGPUImpl::makeImageTexture(uint32_t width,
                                                             uint32_t height,
                                                             uint32_t mipLevelCount,
                                                             const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureWebGPUImpl>(m_device,
                                          m_queue,
                                          width,
                                          height,
                                          mipLevelCount,
                                          imageDataRGBA);
}

class BufferWebGPU : public BufferRing
{
public:
    static std::unique_ptr<BufferWebGPU> Make(wgpu::Device device,
                                              wgpu::Queue queue,
                                              size_t capacityInBytes,
                                              wgpu::BufferUsage usage)
    {
        return std::make_unique<BufferWebGPU>(device, queue, capacityInBytes, usage);
    }

    BufferWebGPU(wgpu::Device device,
                 wgpu::Queue queue,
                 size_t capacityInBytes,
                 wgpu::BufferUsage usage) :
        BufferRing(std::max<size_t>(capacityInBytes, 1)), m_queue(queue)
    {
        wgpu::BufferDescriptor desc = {
            .usage = wgpu::BufferUsage::CopyDst | usage,
            .size = capacityInBytes,
        };
        for (int i = 0; i < gpu::kBufferRingSize; ++i)
        {
            m_buffers[i] = device.CreateBuffer(&desc);
        }
    }

    wgpu::Buffer submittedBuffer() const { return m_buffers[submittedBufferIdx()]; }

protected:
    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override { return shadowBuffer(); }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        write_buffer(m_queue, m_buffers[bufferIdx], shadowBuffer(), mapSizeInBytes);
    }

    const wgpu::Queue m_queue;
    wgpu::Buffer m_buffers[kBufferRingSize];
};

// GL TextureFormat to use for a texture that polyfills a storage buffer.
static wgpu::TextureFormat storage_texture_format(gpu::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case gpu::StorageBufferStructure::uint32x4:
            return wgpu::TextureFormat::RGBA32Uint;
        case gpu::StorageBufferStructure::uint32x2:
            return wgpu::TextureFormat::RG32Uint;
        case gpu::StorageBufferStructure::float32x4:
            return wgpu::TextureFormat::RGBA32Float;
    }
    RIVE_UNREACHABLE();
}

// Buffer ring with a texture used to polyfill storage buffers when they are disabled.
class StorageTextureBufferWebGPU : public BufferWebGPU
{
public:
    StorageTextureBufferWebGPU(wgpu::Device device,
                               wgpu::Queue queue,
                               size_t capacityInBytes,
                               gpu::StorageBufferStructure bufferStructure) :
        BufferWebGPU(device,
                     queue,
                     gpu::StorageTextureBufferSize(capacityInBytes, bufferStructure),
                     wgpu::BufferUsage::CopySrc),
        m_bufferStructure(bufferStructure)
    {
        // Create a texture to mirror the buffer contents.
        auto [textureWidth, textureHeight] =
            gpu::StorageTextureSize(this->capacityInBytes(), bufferStructure);

        wgpu::TextureDescriptor desc{
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .size = {textureWidth, textureHeight},
            .format = storage_texture_format(bufferStructure),
        };

        m_texture = device.CreateTexture(&desc);
        m_textureView = m_texture.CreateView();
    }

    void updateTextureFromBuffer(size_t bindingSizeInBytes,
                                 size_t offsetSizeInBytes,
                                 wgpu::CommandEncoder encoder) const
    {
        auto [updateWidth, updateHeight] =
            gpu::StorageTextureSize(bindingSizeInBytes, m_bufferStructure);
        wgpu::ImageCopyBuffer srcBuffer = {
            .layout =
                {
                    .offset = offsetSizeInBytes,
                    .bytesPerRow = (STORAGE_TEXTURE_WIDTH *
                                    gpu::StorageBufferElementSizeInBytes(m_bufferStructure)),
                },
            .buffer = submittedBuffer(),
        };
        wgpu::ImageCopyTexture dstTexture = {
            .texture = m_texture,
            .origin = {0, 0, 0},
        };
        wgpu::Extent3D copySize = {
            .width = updateWidth,
            .height = updateHeight,
        };
        encoder.CopyBufferToTexture(&srcBuffer, &dstTexture, &copySize);
    }

    wgpu::TextureView textureView() const { return m_textureView; }

private:
    const StorageBufferStructure m_bufferStructure;
    wgpu::Texture m_texture;
    wgpu::TextureView m_textureView;
};

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makeUniformBufferRing(
    size_t capacityInBytes)
{
    // Uniform blocks must be multiples of 256 bytes in size.
    capacityInBytes = std::max<size_t>(capacityInBytes, 256);
    assert(capacityInBytes % 256 == 0);
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacityInBytes,
                                          wgpu::BufferUsage::Uniform);
}

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makeStorageBufferRing(
    size_t capacityInBytes,
    gpu::StorageBufferStructure bufferStructure)
{
    if (m_contextOptions.disableStorageBuffers)
    {
        return std::make_unique<StorageTextureBufferWebGPU>(m_device,
                                                            m_queue,
                                                            capacityInBytes,
                                                            bufferStructure);
    }
    else
    {
        return std::make_unique<BufferWebGPU>(m_device,
                                              m_queue,
                                              capacityInBytes,
                                              wgpu::BufferUsage::Storage);
    }
}

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makeVertexBufferRing(size_t capacityInBytes)
{
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacityInBytes,
                                          wgpu::BufferUsage::Vertex);
}

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makeTextureTransferBufferRing(
    size_t capacityInBytes)
{
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacityInBytes,
                                          wgpu::BufferUsage::CopySrc);
}

void PLSRenderContextWebGPUImpl::resizeGradientTexture(uint32_t width, uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
                 wgpu::TextureUsage::CopyDst,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    m_gradientTexture = m_device.CreateTexture(&desc);
    m_gradientTextureView = m_gradientTexture.CreateView();
}

void PLSRenderContextWebGPUImpl::resizeTessellationTexture(uint32_t width, uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::RGBA32Uint,
    };

    m_tessVertexTexture = m_device.CreateTexture(&desc);
    m_tessVertexTextureView = m_tessVertexTexture.CreateView();
}

wgpu::RenderPipeline PLSRenderContextWebGPUImpl::makePLSDrawPipeline(
    rive::gpu::DrawType drawType,
    wgpu::TextureFormat framebufferFormat,
    wgpu::ShaderModule vertexShader,
    wgpu::ShaderModule fragmentShader,
    EmJsHandle* pipelineJSHandleIfNeeded)
{
    std::vector<wgpu::VertexAttribute> attrs;
    std::vector<wgpu::VertexBufferLayout> vertexBufferLayouts;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::outerCurvePatches:
        {
            attrs = {
                {
                    .format = wgpu::VertexFormat::Float32x4,
                    .offset = 0,
                    .shaderLocation = 0,
                },
                {
                    .format = wgpu::VertexFormat::Float32x4,
                    .offset = 4 * sizeof(float),
                    .shaderLocation = 1,
                },
            };

            vertexBufferLayouts = {
                {
                    .arrayStride = sizeof(gpu::PatchVertex),
                    .stepMode = wgpu::VertexStepMode::Vertex,
                    .attributeCount = std::size(attrs),
                    .attributes = attrs.data(),
                },
            };
            break;
        }
        case DrawType::interiorTriangulation:
        {
            attrs = {
                {
                    .format = wgpu::VertexFormat::Float32x3,
                    .offset = 0,
                    .shaderLocation = 0,
                },
            };

            vertexBufferLayouts = {
                {
                    .arrayStride = sizeof(gpu::TriangleVertex),
                    .stepMode = wgpu::VertexStepMode::Vertex,
                    .attributeCount = std::size(attrs),
                    .attributes = attrs.data(),
                },
            };
            break;
        }
        case DrawType::imageRect:
            RIVE_UNREACHABLE();
        case DrawType::imageMesh:
            attrs = {
                {
                    .format = wgpu::VertexFormat::Float32x2,
                    .offset = 0,
                    .shaderLocation = 0,
                },
                {
                    .format = wgpu::VertexFormat::Float32x2,
                    .offset = 0,
                    .shaderLocation = 1,
                },
            };

            vertexBufferLayouts = {
                {
                    .arrayStride = sizeof(float) * 2,
                    .stepMode = wgpu::VertexStepMode::Vertex,
                    .attributeCount = 1,
                    .attributes = &attrs[0],
                },
                {
                    .arrayStride = sizeof(float) * 2,
                    .stepMode = wgpu::VertexStepMode::Vertex,
                    .attributeCount = 1,
                    .attributes = &attrs[1],
                },
            };
            break;
        case DrawType::gpuAtomicInitialize:
        case DrawType::gpuAtomicResolve:
        case DrawType::stencilClipReset:
            RIVE_UNREACHABLE();
    }

    wgpu::ColorTargetState colorTargets[] = {
        {.format = framebufferFormat},
        {.format = wgpu::TextureFormat::R32Uint},
        {.format = framebufferFormat},
        {.format = wgpu::TextureFormat::R32Uint},
    };
    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);

    wgpu::FragmentState fragmentState = {
        .module = fragmentShader,
        .entryPoint = "main",
        .targetCount = static_cast<size_t>(
            m_contextOptions.plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage ? 1
                                                                                              : 4),
        .targets = colorTargets,
    };

    wgpu::RenderPipelineDescriptor desc = {
        .layout = m_drawPipelineLayout,
        .vertex =
            {
                .module = vertexShader,
                .entryPoint = "main",
                .bufferCount = std::size(vertexBufferLayouts),
                .buffers = vertexBufferLayouts.data(),
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
                .frontFace = m_frontFaceForOnScreenDraws,
                .cullMode =
                    DrawTypeIsImageDraw(drawType) ? wgpu::CullMode::None : wgpu::CullMode::Back,
            },
        .fragment = &fragmentState,
    };

    return m_device.CreateRenderPipeline(&desc);
}

wgpu::RenderPassEncoder PLSRenderContextWebGPUImpl::makePLSRenderPass(
    wgpu::CommandEncoder encoder,
    const PLSRenderTargetWebGPU* renderTarget,
    wgpu::LoadOp loadOp,
    const wgpu::Color& clearColor,
    EmJsHandle* renderPassJSHandleIfNeeded)
{
    wgpu::RenderPassColorAttachment plsAttachments[4] = {
        {
            // framebuffer
            .view = renderTarget->m_targetTextureView,
            .loadOp = loadOp,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = clearColor,
        },
        {
            // clip
            .view = renderTarget->m_clipTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Discard,
            .clearValue = {},
        },
        {
            // scratchColor
            .view = renderTarget->m_scratchColorTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Discard,
            .clearValue = {},
        },
        {
            // coverage
            .view = renderTarget->m_coverageTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Discard,
            .clearValue = {},
        },
    };
    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);

    wgpu::RenderPassDescriptor passDesc = {
        .colorAttachmentCount = static_cast<size_t>(
            m_contextOptions.plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage ? 1
                                                                                              : 4),
        .colorAttachments = plsAttachments,
    };

    return encoder.BeginRenderPass(&passDesc);
}

static wgpu::Buffer webgpu_buffer(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<const BufferWebGPU*>(bufferRing)->submittedBuffer();
}

template <typename HighLevelStruct>
void update_webgpu_storage_texture(const BufferRing* bufferRing,
                                   size_t bindingCount,
                                   size_t firstElement,
                                   wgpu::CommandEncoder encoder)
{
    assert(bufferRing != nullptr);
    auto storageTextureBuffer = static_cast<const StorageTextureBufferWebGPU*>(bufferRing);
    storageTextureBuffer->updateTextureFromBuffer(bindingCount * sizeof(HighLevelStruct),
                                                  firstElement * sizeof(HighLevelStruct),
                                                  encoder);
}

wgpu::TextureView webgpu_storage_texture_view(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<const StorageTextureBufferWebGPU*>(bufferRing)->textureView();
}

void PLSRenderContextWebGPUImpl::flush(const FlushDescriptor& desc)
{
    auto* renderTarget = static_cast<const PLSRenderTargetWebGPU*>(desc.renderTarget);
    wgpu::CommandEncoder encoder = m_device.CreateCommandEncoder();

    // If storage buffers are disabled, copy their contents to textures.
    if (m_contextOptions.disableStorageBuffers)
    {
        if (desc.pathCount > 0)
        {
            update_webgpu_storage_texture<gpu::PathData>(pathBufferRing(),
                                                         desc.pathCount,
                                                         desc.firstPath,
                                                         encoder);
            update_webgpu_storage_texture<gpu::PaintData>(paintBufferRing(),
                                                          desc.pathCount,
                                                          desc.firstPaint,
                                                          encoder);
            update_webgpu_storage_texture<gpu::PaintAuxData>(paintAuxBufferRing(),
                                                             desc.pathCount,
                                                             desc.firstPaintAux,
                                                             encoder);
        }
        if (desc.contourCount > 0)
        {
            update_webgpu_storage_texture<gpu::ContourData>(contourBufferRing(),
                                                            desc.contourCount,
                                                            desc.firstContour,
                                                            encoder);
        }
    }

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        wgpu::BindGroupEntry bindingEntries[] = {
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .buffer = webgpu_buffer(flushUniformBufferRing()),
                .offset = desc.flushUniformDataOffsetInBytes,
            },
        };

        wgpu::BindGroupDescriptor bindGroupDesc = {
            .layout = m_colorRampPipeline->bindGroupLayout(),
            .entryCount = std::size(bindingEntries),
            .entries = bindingEntries,
        };

        wgpu::BindGroup bindings = m_device.CreateBindGroup(&bindGroupDesc);

        wgpu::RenderPassColorAttachment attachment = {
            .view = m_gradientTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

        wgpu::RenderPassDescriptor gradPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

        wgpu::RenderPassEncoder gradPass = encoder.BeginRenderPass(&gradPassDesc);
        gradPass.SetViewport(0.f,
                             static_cast<double>(desc.complexGradRowsTop),
                             gpu::kGradTextureWidth,
                             static_cast<float>(desc.complexGradRowsHeight),
                             0.0,
                             1.0);
        gradPass.SetPipeline(m_colorRampPipeline->renderPipeline());
        gradPass.SetVertexBuffer(0,
                                 webgpu_buffer(gradSpanBufferRing()),
                                 desc.firstComplexGradSpan * sizeof(gpu::GradientSpan));
        gradPass.SetBindGroup(0, bindings);
        gradPass.Draw(4, desc.complexGradSpanCount, 0, 0);
        gradPass.End();
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        wgpu::ImageCopyBuffer srcBuffer = {
            .layout =
                {
                    .offset = desc.simpleGradDataOffsetInBytes,
                    .bytesPerRow = gpu::kGradTextureWidth * 4,
                },
            .buffer = webgpu_buffer(simpleColorRampsBufferRing()),
        };
        wgpu::ImageCopyTexture dstTexture = {
            .texture = m_gradientTexture,
            .origin = {0, 0, 0},
        };
        wgpu::Extent3D copySize = {
            .width = desc.simpleGradTexelsWidth,
            .height = desc.simpleGradTexelsHeight,
        };
        encoder.CopyBufferToTexture(&srcBuffer, &dstTexture, &copySize);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        wgpu::BindGroupEntry bindingEntries[] = {
            m_contextOptions.disableStorageBuffers ?
                wgpu::BindGroupEntry{
                    .binding = PATH_BUFFER_IDX,
                    .textureView = webgpu_storage_texture_view(pathBufferRing())
                } :
                wgpu::BindGroupEntry{
                    .binding = PATH_BUFFER_IDX,
                    .buffer = webgpu_buffer(pathBufferRing()),
                    .offset = desc.firstPath * sizeof(gpu::PathData),
                },
            m_contextOptions.disableStorageBuffers ?
                wgpu::BindGroupEntry{
                    .binding = CONTOUR_BUFFER_IDX,
                    .textureView = webgpu_storage_texture_view(contourBufferRing()),
                } :
                wgpu::BindGroupEntry{
                    .binding = CONTOUR_BUFFER_IDX,
                    .buffer = webgpu_buffer(contourBufferRing()),
                    .offset = desc.firstContour * sizeof(gpu::ContourData),
                },
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .buffer = webgpu_buffer(flushUniformBufferRing()),
                .offset = desc.flushUniformDataOffsetInBytes,
            },
        };

        wgpu::BindGroupDescriptor bindGroupDesc = {
            .layout = m_tessellatePipeline->bindGroupLayout(),
            .entryCount = std::size(bindingEntries),
            .entries = bindingEntries,
        };

        wgpu::BindGroup bindings = m_device.CreateBindGroup(&bindGroupDesc);

        wgpu::RenderPassColorAttachment attachment{
            .view = m_tessVertexTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

        wgpu::RenderPassDescriptor tessPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

        wgpu::RenderPassEncoder tessPass = encoder.BeginRenderPass(&tessPassDesc);
        tessPass.SetViewport(0.f,
                             0.f,
                             gpu::kTessTextureWidth,
                             static_cast<float>(desc.tessDataHeight),
                             0.0,
                             1.0);
        tessPass.SetPipeline(m_tessellatePipeline->renderPipeline());
        tessPass.SetVertexBuffer(0,
                                 webgpu_buffer(tessSpanBufferRing()),
                                 desc.firstTessVertexSpan * sizeof(gpu::TessVertexSpan));
        tessPass.SetIndexBuffer(m_tessSpanIndexBuffer, wgpu::IndexFormat::Uint16);
        tessPass.SetBindGroup(0, bindings);
        tessPass.DrawIndexed(std::size(gpu::kTessSpanIndices), desc.tessVertexSpanCount, 0, 0, 0);
        tessPass.End();
    }

    wgpu::LoadOp loadOp;
    wgpu::Color clearColor;
    if (desc.colorLoadAction == LoadAction::clear)
    {
        loadOp = wgpu::LoadOp::Clear;
        float cc[4];
        UnpackColorToRGBA32F(desc.clearColor, cc);
        clearColor = {cc[0], cc[1], cc[2], cc[3]};
    }
    else
    {
        loadOp = wgpu::LoadOp::Load;
    }

    EmJsHandle drawPassJSHandle;
    wgpu::RenderPassEncoder drawPass =
        makePLSRenderPass(encoder, renderTarget, loadOp, clearColor, &drawPassJSHandle);
    drawPass.SetViewport(0.f, 0.f, renderTarget->width(), renderTarget->height(), 0.0, 1.0);

    bool needsPLSTextureBindings = m_contextOptions.plsType == PixelLocalStorageType::subpassLoad;
    if (needsPLSTextureBindings)
    {
        wgpu::BindGroupEntry plsTextureBindingEntries[] = {
            {
                .binding = COLOR_PLANE_IDX,
                .textureView = renderTarget->m_targetTextureView,
            },
            {
                .binding = CLIP_PLANE_IDX,
                .textureView = renderTarget->m_clipTextureView,
            },
            {
                .binding = SCRATCH_COLOR_PLANE_IDX,
                .textureView = renderTarget->m_scratchColorTextureView,
            },
            {
                .binding = COVERAGE_PLANE_IDX,
                .textureView = renderTarget->m_coverageTextureView,
            },
        };

        wgpu::BindGroupDescriptor plsTextureBindingsGroupDesc = {
            .layout = m_drawBindGroupLayouts[PLS_TEXTURE_BINDINGS_SET],
            .entryCount = std::size(plsTextureBindingEntries),
            .entries = plsTextureBindingEntries,
        };

        wgpu::BindGroup plsTextureBindings = m_device.CreateBindGroup(&plsTextureBindingsGroupDesc);
        drawPass.SetBindGroup(PLS_TEXTURE_BINDINGS_SET, plsTextureBindings);
    }

    if (m_contextOptions.plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
    {
        enable_shader_pixel_local_storage_ext(drawPass, true);

        // Draw the load action for EXT_shader_pixel_local_storage.
        std::array<float, 4> clearColor;
        LoadStoreActionsEXT loadActions = gpu::BuildLoadActionsEXT(desc, &clearColor);
        const LoadStoreEXTPipeline& loadPipeline =
            m_loadStoreEXTPipelines
                .try_emplace(loadActions, this, loadActions, renderTarget->framebufferFormat())
                .first->second;

        if (loadActions & LoadStoreActionsEXT::clearColor)
        {
            void* uniformData = m_loadStoreEXTUniforms->mapBuffer(sizeof(clearColor));
            memcpy(uniformData, clearColor.data(), sizeof(clearColor));
            m_loadStoreEXTUniforms->unmapAndSubmitBuffer();

            wgpu::BindGroupEntry uniformBindingEntries[] = {
                {
                    .binding = 0,
                    .buffer = webgpu_buffer(m_loadStoreEXTUniforms.get()),
                },
            };

            wgpu::BindGroupDescriptor uniformBindGroupDesc = {
                .layout = loadPipeline.bindGroupLayout(),
                .entryCount = std::size(uniformBindingEntries),
                .entries = uniformBindingEntries,
            };

            wgpu::BindGroup uniformBindings = m_device.CreateBindGroup(&uniformBindGroupDesc);
            drawPass.SetBindGroup(0, uniformBindings);
        }

        drawPass.SetPipeline(loadPipeline.renderPipeline(renderTarget->framebufferFormat()));
        drawPass.Draw(4);
    }

    drawPass.SetBindGroup(SAMPLER_BINDINGS_SET, m_samplerBindings);

    wgpu::BindGroupEntry perFlushBindingEntries[] = {
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .textureView = m_tessVertexTextureView,
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .textureView = m_gradientTextureView,
        },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PATH_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(pathBufferRing())
            } :
            wgpu::BindGroupEntry{
                .binding = PATH_BUFFER_IDX,
                .buffer = webgpu_buffer(pathBufferRing()),
                .offset = desc.firstPath * sizeof(gpu::PathData),
            },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PAINT_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(paintBufferRing()),
            } :
            wgpu::BindGroupEntry{
                .binding = PAINT_BUFFER_IDX,
                .buffer = webgpu_buffer(paintBufferRing()),
                .offset = desc.firstPaint * sizeof(gpu::PaintData),
            },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(paintAuxBufferRing()),
            } :
            wgpu::BindGroupEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .buffer = webgpu_buffer(paintAuxBufferRing()),
                .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
            },
        m_contextOptions.disableStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(contourBufferRing()),
            } :
            wgpu::BindGroupEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .buffer = webgpu_buffer(contourBufferRing()),
                .offset = desc.firstContour * sizeof(gpu::ContourData),
            },
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(flushUniformBufferRing()),
            .offset = desc.flushUniformDataOffsetInBytes,
        },
        {
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(imageDrawUniformBufferRing()),
            .size = sizeof(gpu::ImageDrawUniforms),
        },
    };

    wgpu::BindGroupDescriptor perFlushBindGroupDesc = {
        .layout = m_drawBindGroupLayouts[PER_FLUSH_BINDINGS_SET],
        .entryCount = std::size(perFlushBindingEntries),
        .entries = perFlushBindingEntries,
    };

    wgpu::BindGroup perFlushBindings = m_device.CreateBindGroup(&perFlushBindGroupDesc);

    // Execute the DrawList.
    bool needsNewBindings = true;
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            continue;
        }

        DrawType drawType = batch.drawType;

        // Bind the appropriate image texture, if any.
        wgpu::TextureView imageTextureView = m_nullImagePaintTextureView;
        if (auto imageTexture = static_cast<const PLSTextureWebGPUImpl*>(batch.imageTexture))
        {
            imageTextureView = imageTexture->textureView();
            needsNewBindings = true;
        }

        if (needsNewBindings ||
            // Image draws always re-bind because they update the dynamic offset to their uniforms.
            drawType == DrawType::imageRect || drawType == DrawType::imageMesh)
        {
            drawPass.SetBindGroup(PER_FLUSH_BINDINGS_SET,
                                  perFlushBindings,
                                  1,
                                  &batch.imageDrawDataOffset);

            wgpu::BindGroupEntry perDrawBindingEntries[] = {
                {
                    .binding = IMAGE_TEXTURE_IDX,
                    .textureView = imageTextureView,
                },
            };

            wgpu::BindGroupDescriptor perDrawBindGroupDesc = {
                .layout = m_drawBindGroupLayouts[PER_DRAW_BINDINGS_SET],
                .entryCount = std::size(perDrawBindingEntries),
                .entries = perDrawBindingEntries,
            };

            wgpu::BindGroup perDrawBindings = m_device.CreateBindGroup(&perDrawBindGroupDesc);
            drawPass.SetBindGroup(PER_DRAW_BINDINGS_SET, perDrawBindings, 0, nullptr);
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        const DrawPipeline& drawPipeline =
            m_drawPipelines
                .try_emplace(gpu::ShaderUniqueKey(drawType,
                                                  batch.shaderFeatures,
                                                  gpu::InterlockMode::rasterOrdering,
                                                  gpu::ShaderMiscFlags::none),
                             this,
                             drawType,
                             batch.shaderFeatures,
                             m_contextOptions)
                .first->second;
        drawPass.SetPipeline(drawPipeline.renderPipeline(renderTarget->framebufferFormat()));

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                drawPass.SetVertexBuffer(0, m_pathPatchVertexBuffer);
                drawPass.SetIndexBuffer(m_pathPatchIndexBuffer, wgpu::IndexFormat::Uint16);
                drawPass.DrawIndexed(gpu::PatchIndexCount(drawType),
                                     batch.elementCount,
                                     gpu::PatchBaseIndex(drawType),
                                     0,
                                     batch.baseElement);
                break;
            }
            case DrawType::interiorTriangulation:
            {
                drawPass.SetVertexBuffer(0, webgpu_buffer(triangleBufferRing()));
                drawPass.Draw(batch.elementCount, 1, batch.baseElement);
                break;
            }
            case DrawType::imageRect:
                RIVE_UNREACHABLE();
            case DrawType::imageMesh:
            {
                auto vertexBuffer = static_cast<const RenderBufferWebGPUImpl*>(batch.vertexBuffer);
                auto uvBuffer = static_cast<const RenderBufferWebGPUImpl*>(batch.uvBuffer);
                auto indexBuffer = static_cast<const RenderBufferWebGPUImpl*>(batch.indexBuffer);
                drawPass.SetVertexBuffer(0, vertexBuffer->submittedBuffer());
                drawPass.SetVertexBuffer(1, uvBuffer->submittedBuffer());
                drawPass.SetIndexBuffer(indexBuffer->submittedBuffer(), wgpu::IndexFormat::Uint16);
                drawPass.DrawIndexed(batch.elementCount, 1, batch.baseElement);
                break;
            }
            case DrawType::gpuAtomicInitialize:
            case DrawType::gpuAtomicResolve:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }
    }

    if (m_contextOptions.plsType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
    {
        // Draw the store action for EXT_shader_pixel_local_storage.
        LoadStoreActionsEXT actions = LoadStoreActionsEXT::storeColor;
        auto it = m_loadStoreEXTPipelines.try_emplace(actions,
                                                      this,
                                                      actions,
                                                      renderTarget->framebufferFormat());
        LoadStoreEXTPipeline* storePipeline = &it.first->second;
        drawPass.SetPipeline(storePipeline->renderPipeline(renderTarget->framebufferFormat()));
        drawPass.Draw(4);

        enable_shader_pixel_local_storage_ext(drawPass, false);
    }

    drawPass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    m_queue.Submit(1, &commands);
}

std::unique_ptr<PLSRenderContext> PLSRenderContextWebGPUImpl::MakeContext(
    wgpu::Device device,
    wgpu::Queue queue,
    const ContextOptions& contextOptions,
    const gpu::PlatformFeatures& baselinePlatformFeatures)
{
    std::unique_ptr<PLSRenderContextWebGPUImpl> impl;
    switch (contextOptions.plsType)
    {
        case PixelLocalStorageType::subpassLoad:
#ifdef RIVE_WEBGPU
            impl = std::unique_ptr<PLSRenderContextWebGPUImpl>(
                new PLSRenderContextWebGPUVulkan(device,
                                                 queue,
                                                 contextOptions,
                                                 baselinePlatformFeatures));
            break;
#endif
        case PixelLocalStorageType::EXT_shader_pixel_local_storage:
        case PixelLocalStorageType::none:
            impl = std::unique_ptr<PLSRenderContextWebGPUImpl>(
                new PLSRenderContextWebGPUImpl(device,
                                               queue,
                                               contextOptions,
                                               baselinePlatformFeatures));
            break;
    }
    impl->initGPUObjects();
    return std::make_unique<PLSRenderContext>(std::move(impl));
}
} // namespace rive::gpu
